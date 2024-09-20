#define export exports
extern "C" {
#include <qbe/all.h>
}
#undef export

#include <unordered_map>
#include <string>
#include <iostream>
#include <set>
#include <map>
#include <set>
#include <algorithm>


static auto live_variables(Blk* entry, const std::map<Blk*, std::set<Blk*> >& succ,
    const std::map<Blk*, std::set<std::string>>& def, const std::map<Blk*, std::set<std::string>>& use) {

    std::map<Blk*, std::set<std::string>> out;
    for (Blk* blk = entry; blk; blk = blk->link) {
        out[blk] = std::set<std::string>();
    }
    bool change = true;

    while (change) {
        change = false;

        for (Blk* blk = entry; blk; blk = blk->link) {
            std::set<std::string> out_new;

            if (!succ.count(blk)) {
                continue;
            }

            for (const auto& succ_blk : succ.at(blk)) {
                std::set<std::string> cur_result;

                std::set_difference(out[succ_blk].begin(), out[succ_blk].end(),
                    def.at(succ_blk).begin(), def.at(succ_blk).end(),
                    std::inserter(cur_result, cur_result.begin()));

                std::set_union(use.at(succ_blk).begin(), use.at(succ_blk).end(),
                    cur_result.begin(), cur_result.end(),
                    std::inserter(cur_result, cur_result.begin()));

                std::set_union(cur_result.begin(), cur_result.end(),
                    out_new.begin(), out_new.end(),
                    std::inserter(out_new, out_new.begin()));
            }

            if (out_new != out[blk]) {
                out[blk] = out_new;
                change = true;
            }
        }
    }

    return out;
}

static std::pair<Blk*, Blk*> prepare_for_entry(Blk* start) {
    Blk* Entry = new Blk();
    Entry->link = start;
    Entry->s1 = start;

    Blk* Exit = new Blk();

    Blk* blk = start;
    for (; blk && blk->link; blk = blk->link);
    blk->link = Exit;
    blk->s1 = Exit;

    return { Entry, blk };
}

static auto construct_def_use(Fn* fn) {
    std::map<Blk*, std::set<std::string>> def, use;
    Blk* last_blk = nullptr;

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        if (blk->jmp.type == Jret0 || blk->jmp.type >= Jretw) {
            last_blk = blk;
        }

        for (int i = 0; i < blk->nins; i++) {
            std::string arg1 = fn->tmp[blk->ins[i].arg[0].val].name;
            std::string arg2 = fn->tmp[blk->ins[i].arg[1].val].name;

            if (std::string{ arg1 } != "" && !def[blk].count(arg1)) {
                use[blk].insert(arg1);
            }

            if (std::string{ arg2 } != "" && !def[blk].count(arg2)) {
                use[blk].insert(arg2);
            }

            if (std::string{ fn->tmp[blk->ins[i].to.val].name } != "") {
                def[blk].insert(fn->tmp[blk->ins[i].to.val].name);
            }
        }
    }

    std::string ret_var = fn->tmp[last_blk->jmp.arg.val].name;

    if (std::string{ ret_var } != "" && !def[last_blk].count(ret_var)) {
        use[last_blk].insert(ret_var);
    }

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        if (!def.count(blk)) {
            def[blk] = std::set<std::string>();
        }

        if (!use.count(blk)) {
            use[blk] = std::set<std::string>();
        }
    }

    return std::make_pair(def, use);
}

static void fill_succ(Blk* entry, std::map<Blk*, std::set<Blk*>>& succ) {
    for (Blk* blk = entry; blk; blk = blk->link) {
        if (blk->s1) {
            succ[blk].insert(blk->s1);
        }
        if (blk->s2) {
            succ[blk].insert(blk->s2);
        }
    }
}

static void clean_up(std::pair<Blk*, Blk*> entry) {
    Blk* blk = entry.first;
    delete entry.first;
    auto tmp = entry.second->link;
    entry.second->link = NULL;
    delete tmp;
}

void fill_def_use_ends(Blk* entry, std::pair<std::map<Blk*, std::set<std::string>>,
    std::map<Blk*, std::set<std::string>>>& def_use) {
    def_use.first[entry] = std::set<std::string>();
    def_use.second[entry] = std::set<std::string>();

    Blk* blk = entry;
    for (; blk && blk->link; blk = blk->link);

    def_use.first[blk] = std::set<std::string>();
    def_use.second[blk] = std::set<std::string>();
}


static void readfn(Fn* fn) {
    auto def_use = construct_def_use(fn);
    auto entry = prepare_for_entry(fn->start);
    fill_def_use_ends(entry.first, def_use);

    std::map<Blk*, std::set<Blk*>> succ;
    fill_succ(entry.first, succ);

    auto out = live_variables(entry.first, succ, def_use.first, def_use.second);
    clean_up(entry);

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        std::cout << "@" << blk->name << std::endl;
        std::cout << "\tlv_out = ";

        for (const auto& out_val : out[blk]) {
            std::cout << "%" << out_val << " ";
        }
        std::cout << std::endl;
    }
}

static void readdat(Dat* dat) {
    (void)dat;
}

int main() {
    parse(stdin, "<stdin>", readdat, readfn);
    freeall();
}