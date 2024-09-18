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


static auto reaching_definitions(Blk* entry, const std::map<Blk*, std::set<Blk*> >& pred,
    const std::map<Blk*, std::set<std::string>>& gen, const std::map<Blk*, std::set<std::string>>& kill) {

    std::map<Blk*, std::set<std::string>> in;
    for (Blk* blk = entry; blk; blk = blk->link) {
        in[blk] = std::set<std::string>();
    }
    bool change = true;

    while (change) {
        change = false;

        for (Blk* blk = entry; blk; blk = blk->link) {
            if (blk == entry) {
                continue;
            }

            std::set<std::string> in_new;

            if (!pred.count(blk)) {
                continue;
            }

            for (const auto& pred_blk : pred.at(blk)) {
                std::set<std::string> cur_result;

                std::set_difference(in[pred_blk].begin(), in[pred_blk].end(),
                    kill.at(pred_blk).begin(), kill.at(pred_blk).end(),
                    std::inserter(cur_result, cur_result.begin()));

                std::set_union(gen.at(pred_blk).begin(), gen.at(pred_blk).end(),
                    cur_result.begin(), cur_result.end(),
                    std::inserter(cur_result, cur_result.begin()));

                std::set_union(cur_result.begin(), cur_result.end(),
                    in_new.begin(), in_new.end(),
                    std::inserter(in_new, in_new.begin()));
            }

            if (in_new != in[blk]) {
                in[blk] = in_new;
                change = true;
            }
        }
    }

    return in;
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

static auto construct_gen_kill(Fn* fn) {
    std::map<Blk*, std::set<std::string>> definition_per_block;
    std::map<std::string, std::set<std::string>> all_variable_occurences;

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        for (int i = 0; i < blk->nins; i++) {
            if (Tmp0 > blk->ins[i].to.val) {
                continue;
            }
            std::string variable_full_name = "@" + std::string{ blk->name } + "%" + std::string{ fn->tmp[blk->ins[i].to.val].name };

            definition_per_block[blk].insert(fn->tmp[blk->ins[i].to.val].name);
            all_variable_occurences[std::string{ fn->tmp[blk->ins[i].to.val].name }].insert(variable_full_name);
        }
    }

    std::map<Blk*, std::set<std::string>> gen, kill;

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        for (const auto& variable : definition_per_block[blk]) {
            gen[blk].insert("@" + std::string{ blk->name } + "%" + std::string{ variable });
        }

        for (const auto& variable : definition_per_block[blk]) {
            for (const auto& full_name : all_variable_occurences[variable]) {
                std::string variable_name = "@" + std::string{ blk->name } + "%" + variable;

                if (full_name == variable_name) {
                    continue;
                }

                kill[blk].insert(full_name);
            }
        }
    }

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        if (!gen.count(blk)) {
            gen[blk] = std::set<std::string>();
        }

        if (!kill.count(blk)) {
            kill[blk] = std::set<std::string>();
        }
    }

    return std::make_pair(gen, kill);
}

static void fill_pred(Blk* entry, std::map<Blk*, std::set<Blk*>>& pred) {
    for (Blk* blk = entry; blk; blk = blk->link) {
        if (blk->s1) {
            pred[blk->s1].insert(blk);
        }
        if (blk->s2) {
            pred[blk->s2].insert(blk);
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

void fill_gen_kill_ends(Blk* entry, std::pair<std::map<Blk*, std::set<std::string>>,
    std::map<Blk*, std::set<std::string>>>& gen_kill) {
    gen_kill.first[entry] = std::set<std::string>();
    gen_kill.second[entry] = std::set<std::string>();

    Blk* blk = entry;
    for (; blk && blk->link; blk = blk->link);

    gen_kill.first[blk] = std::set<std::string>();
    gen_kill.second[blk] = std::set<std::string>();
}


static void readfn(Fn* fn) {
    auto gen_kill = construct_gen_kill(fn);
    auto entry = prepare_for_entry(fn->start);
    fill_gen_kill_ends(entry.first, gen_kill);

    std::map<Blk*, std::set<Blk*>> pred;
    fill_pred(entry.first, pred);

    auto in = reaching_definitions(entry.first, pred, gen_kill.first, gen_kill.second);
    clean_up(entry);

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        std::cout << "@" << blk->name << std::endl;
        std::cout << "\trd_in = ";

        for (const auto& in_val : in[blk]) {
            std::cout << in_val << " ";
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