#define export exports
extern "C" {
#include <qbe/all.h>
}
#undef export

#include <stdio.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>


static void readfn(Fn* fn) {
    std::unordered_map<std::string, std::unordered_set<std::string>> definition_per_block;
    std::unordered_map<std::string, std::unordered_set<std::string>> all_variable_occurences;

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        for (int i = 0; i < blk->nins; i++) {
            if (Tmp0 > blk->ins[i].to.val) {
                continue;
            }
            std::string variable_full_name = "@" + std::string{blk->name} + "%" + std::string{fn->tmp[blk->ins[i].to.val].name};

            definition_per_block[std::string{ blk->name }].insert(fn->tmp[blk->ins[i].to.val].name);
            all_variable_occurences[std::string{ fn->tmp[blk->ins[i].to.val].name }].insert(variable_full_name);
        }
    }

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        std::cout << "@" << blk->name << std::endl;
        std::cout << "\tgen = ";

        for (const auto& variable : definition_per_block[std::string{ blk->name }]) {
            std::cout << "@" << blk->name << "%" << variable << " ";
        }

        std::cout << std::endl << "\tkill = ";

        for (const auto& variable : definition_per_block[std::string{ blk->name }]) {
            for (const auto& full_names : all_variable_occurences[variable]) {
                if (full_names == "@" + std::string{ blk->name } + "%" + variable) {
                    continue;
                }

                std::cout << full_names << " ";
            }
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