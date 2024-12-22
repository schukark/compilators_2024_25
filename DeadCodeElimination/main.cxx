#ifdef __cplusplus
#define export exports
extern "C" {
#include <qbe/all.h>
}
#undef export
#else
#include <qbe/all.h>
#endif

#include <stdio.h>
#include <deque>
#include <variant>
#include <set>
#include <string>
#include <map>
#include <iostream>
#include <vector>
#include <algorithm>

enum op_type {
    ins = 1, phi = 2, jump = 4, none = 8,
};

struct Item {
    enum op_type op;
    Blk* block;
    std::variant<Blk*, Ins*, Phi*, std::nullptr_t> item;
    // holds either:
    // 1. Blk* jump information
    // 2. Ins* instruction information
    // 3. Phi* phi function information

    Item() : op(op_type::none), block(nullptr), item(nullptr) {}
    Item(enum op_type op, Blk* block, Blk* item) : op(op), block(block), item(item) {}
    Item(enum op_type op, Blk* block, Ins* item) : op(op), block(block), item(item) {}
    Item(enum op_type op, Blk* block, Phi* item) : op(op), block(block), item(item) {}
};

class DeadCodeEliminator {
public:
    DeadCodeEliminator(Fn* fn) : fn(fn) {
        construct_definitions_map();
        construct_rev_dom();
        construct_rev_idom();
        construct_rdf();

    }

    void eliminate() {
        auto [marked_ins, marked_jmp, marked_phi, marked_blk] = mark();
        sweep(marked_ins, marked_jmp, marked_phi, marked_blk);
    }
private:
    std::tuple<std::set<Ins*>, std::set<Blk*>, std::set<Phi*>, std::set<std::string>> mark() {
        std::deque<Item> worklist;
        std::set<Ins*> marked_ins;
        std::set<Blk*> marked_jmp;
        std::set<Phi*> marked_phi;
        std::set<std::string> marked_blk;

        for (Blk* blk = fn->start; blk; blk = blk->link) {
            for (Ins* ins = blk->ins; ins < &blk->ins[blk->nins]; ins++) {
                if (is_memory_or_call(ins)) {
                    worklist.push_back(Item(op_type::ins, blk, ins));
                    marked_blk.insert(blk->name);
                }
            }

            if (isret(blk->jmp.type)) {
                worklist.push_back(Item(op_type::jump, blk, blk));
                marked_blk.insert(blk->name);
            }

            if (blk->jmp.type == Jjmp && !isret(blk->s1->jmp.type) && blk->s1->nins == 0 && !blk->s1->phi) {
                marked_jmp.insert(blk);
            }

            if (!rev_idom[blk]) {
                worklist.push_back(Item(op_type::jump, blk, blk));
            }
        }


        while (!worklist.empty()) {
            auto top = worklist.back();
            worklist.pop_back();

            if (top.op == op_type::none) {
                continue;
            }

            if (top.op == op_type::ins) {
                auto ins = std::get<Ins*>(top.item);

                if (marked_ins.find(ins) != marked_ins.end()) {
                    continue;
                }

                marked_blk.insert(top.block->name);
                marked_ins.insert(ins);

                if (ins->op == Ocall || ins->op == Ovacall) {
                    Ins* start = ins;
                    for (; start > top.block->ins; start--) {
                        if (!isarg((start - 1)->op)) {
                            break;
                        }
                    }

                    while (start < ins) {
                        worklist.push_back(Item(op_type::ins, top.block, start));
                        start++;
                    }
                }
                else {
                    int argc = 1;
                    if (ins->arg[1].type > 0 || ins->arg[1].val >= Tmp0) {
                        argc += 1;
                    }

                    for (int i = 0; i < argc; i++) {
                        if (ins->arg[i].val < Tmp0) {
                            continue;
                        }

                        std::string name = fn->tmp[ins->arg[i].val].name;
                        // debug("Ins arg " << i << " " << name);
                        worklist.push_back(definitions[name]);
                    }
                }
            }
            else if (top.op == op_type::phi) {
                auto phi = std::get<Phi*>(top.item);

                if (marked_phi.find(phi) != marked_phi.end()) {
                    continue;
                }

                marked_blk.insert(top.block->name);
                marked_phi.insert(phi);

                for (int i = 0; i < phi->narg; i++) {
                    if (phi->arg[i].val < Tmp0) {
                        continue;
                    }

                    std::string name = fn->tmp[phi->arg[i].val].name;
                    worklist.push_back(definitions[name]);

                    if (marked_jmp.find(phi->blk[i]) != marked_jmp.end()) {
                        continue;
                    }

                    worklist.push_back(Item(op_type::jump, phi->blk[i], phi->blk[i]));
                }
            }
            else if (top.op == op_type::jump) {
                auto jmp = std::get<Blk*>(top.item);

                if (marked_jmp.find(jmp) != marked_jmp.end()) {
                    continue;
                }

                marked_blk.insert(top.block->name);
                marked_jmp.insert(jmp);

                if (jmp->jmp.type <= 0 || (jmp->jmp.arg.type <= 0 && jmp->jmp.arg.val < Tmp0)) {
                    continue;
                }

                std::string name = fn->tmp[jmp->jmp.arg.val].name;
                worklist.push_back(definitions[name]);
            }


            for (const auto& block : rdf[top.block]) {
                if (marked_jmp.find(block) != marked_jmp.end()) {
                    continue;
                }
                if (block->jmp.type == Jjmp) {
                    continue;
                }
                worklist.push_back(Item(op_type::jump, block, block));
            }
        }

        return { marked_ins, marked_jmp, marked_phi, marked_blk };
    }

    void sweep(std::set<Ins*> marked_ins, std::set<Blk*> marked_jmp, std::set<Phi*> marked_phi, std::set<std::string> marked_blk) {
        // print_set_name(marked_jmp);
        // print_set(marked_blk);

        for (Blk* blk = fn->start; blk; blk = blk->link) {
            for (Ins* ins = blk->ins; ins < &blk->ins[blk->nins]; ins++) {
                if (marked_ins.find(ins) != marked_ins.end()) {
                    continue;
                }

                make_useless(ins);
            }

            Phi* prev = nullptr;
            for (Phi* phi = blk->phi; phi; phi = phi->link) {
                if (marked_phi.find(phi) != marked_phi.end()) {
                    prev = phi;
                    continue;
                }

                if (!prev) {
                    blk->phi = phi->link;
                }
                else {
                    prev->link = phi->link;
                }
            }

            if (marked_jmp.find(blk) != marked_jmp.end()) {
                continue;
            }

            if (blk->jmp.type == Jjmp && marked_blk.find(blk->s1->name) != marked_blk.end()) {
                continue;
            }

            blk->jmp.type = Jjmp;
            blk->jmp.arg = R;

            Blk* tmp = rev_idom[blk];

            while (tmp && marked_jmp.find(tmp) == marked_jmp.end()) {
                tmp = rev_idom[tmp];
            }

            blk->s1 = tmp;
            blk->s2 = nullptr;
        }
    }

    void construct_definitions_map() {
        for (Blk* blk = fn->start; blk; blk = blk->link) {
            for (Ins* ins = blk->ins; ins < &blk->ins[blk->nins]; ins++) {
                std::string name = fn->tmp[ins->to.val].name;

                if (name.size() == 0) {
                    continue;
                }

                definitions[name] = Item(op_type::ins, blk, ins);
            }

            for (Phi* phi = blk->phi; phi; phi = phi->link) {
                std::string name = fn->tmp[phi->to.val].name;

                if (name.size() == 0) {
                    continue;
                }

                definitions[name] = Item(op_type::phi, blk, phi);
            }
        }
    }

    static bool is_memory_or_call(Ins* ins) {
        return isstore(ins->op)
            || (ins->op == Ocall || ins->op == Ovacall);
    }

    static void inline make_useless(Ins* ins) {
        ins->op = Onop;
        ins->to = R;
        ins->arg[0] = R;
        ins->arg[1] = R;
    }

    void construct_rev_dom() {
        std::set<Blk*> last;
        std::map<Blk*, std::set<Blk*>> pred;

        std::set<Blk*> all_blks;
        for (Blk* blk = fn->start; blk; blk = blk->link) {
            all_blks.insert(blk);

            if (isret(blk->jmp.type)) {
                last.insert(blk);
            }

            if (blk->s1) {
                pred[blk].insert(blk->s1);
            }

            if (blk->s2) {
                pred[blk].insert(blk->s2);
            }
        }

        std::map<Blk*, std::set<Blk*>> out;
        for (Blk* blk = fn->start; blk; blk = blk->link) {
            out[blk] = all_blks;
        }

        for (const auto& last_blk : last) {
            out[last_blk] = std::set<Blk*>{ last_blk };
        }

        bool changed = true;
        int count = 0;
        while (changed) {
            changed = false;
            for (Blk* blk = fn->start; blk; blk = blk->link) {
                if (last.find(blk) != last.end()) {
                    continue;
                }
                std::set<Blk*> in, new_out;
                in = all_blks;

                for (const auto& p : pred[blk]) {
                    decltype(in) new_in;
                    std::set_intersection(in.begin(), in.end(),
                        out[p].begin(), out[p].end(),
                        std::inserter(new_in, new_in.end()));
                    in = new_in;
                }

                new_out = std::move(in);
                new_out.insert(blk);

                if (new_out != out[blk]) {
                    changed = true;
                    out[blk] = new_out;
                }
            }
        }

        rev_dom = std::move(out);
    }
    void numerate(Blk* current, int& counter, const std::map<Blk*, std::set<Blk*>>& succ, std::set<Blk*>& visited) {
        visited.insert(current);
        numeration[current] = counter--;

        if (succ.find(current) == succ.end()) {
            numeration[current] = counter--;
            return;
        }

        for (const auto& succ_blk : succ.at(current)) {
            if (visited.find(succ_blk) != visited.end()) {
                continue;
            }

            numerate(succ_blk, counter, succ, visited);
        }

    }

    void construct_rev_idom() {
        std::map<Blk*, std::set<Blk*>> succ;
        std::set<Blk*> last;

        for (Blk* blk = fn->start; blk; blk = blk->link) {
            if (isret(blk->jmp.type)) {
                last.insert(blk);
            }

            if (blk->s1) {
                succ[blk->s1].insert(blk);
            }

            if (blk->s2) {
                succ[blk->s2].insert(blk);
            }
        }

        std::set<Blk*> visited;
        int counter = fn->nblk;

        for (const auto& last_blk : last) {
            numerate(last_blk, counter, succ, visited);
        }

        numeration.erase(nullptr);

        for (const auto& [key, rev_doms] : rev_dom) {
            Blk* idom = nullptr;

            for (const auto& it : rev_doms) {
                if (!idom && it != key) {
                    idom = it;
                }
                else if (idom && numeration[it] < numeration[idom] && it != key) {
                    idom = it;
                }
            }
            if (idom) {
                rev_idom[key] = idom;
            }
        }
    }

    void construct_rdf() {
        for (Blk* blk = fn->start; blk; blk = blk->link) {
            rdf[blk] = std::set<Blk*>();
        }

        for (Blk* blk = fn->start; blk; blk = blk->link) {
            int npred = !!blk->s1 + !!blk->s2;

            if (npred <= 1) {
                continue;
            }

            std::set<Blk*> pred;
            if (blk->s1) pred.insert(blk->s1);
            if (blk->s2) pred.insert(blk->s2);

            for (const auto& pred_blk : pred) {
                Blk* r = pred_blk;

                if (rev_idom.find(pred_blk) == rev_idom.end() || !rev_idom[pred_blk]) {
                    continue;
                }

                while (r != rev_idom[blk]) {
                    rdf[r].insert(blk);
                    r = rev_idom[r];
                }
            }
        }
    }

public:

private:
    std::map<std::string, Item> definitions;
    std::map<Blk*, std::set<Blk*>> rev_dom;
    std::map<Blk*, int> numeration;
    std::map<Blk*, Blk*> rev_idom;
    std::map<Blk*, std::set<Blk*>> rdf;
    Fn* fn;
};

static void readfn(Fn* fn) {
    fillrpo(fn); // Traverses the CFG in reverse post-order, filling blk->id.
    fillpreds(fn);
    filluse(fn);
    ssa(fn);

    DeadCodeEliminator(fn).eliminate();

    fillpreds(fn); // Either call this, or keep track of preds manually when rewriting branches.
    fillrpo(fn); // As a side effect, fillrpo removes any unreachable blocks.
    printfn(fn, stdout);
}

static void readdat(Dat* dat) {
    (void)dat;
}

int main() {
    parse(stdin, "<stdin>", readdat, readfn);
    freeall();
}