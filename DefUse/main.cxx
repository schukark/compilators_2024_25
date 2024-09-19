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
    std::unordered_map<std::string, std::unordered_set<std::string>> def, use;

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        for (int i = 0; i < blk->nins; i++) {
            if (Tmp0 > blk->ins[i].to.val) {
                continue;
            }

            if (std::string{ fn->tmp[blk->ins[i].to.val].name } != "") {
                def[std::string{ blk->name }].insert(fn->tmp[blk->ins[i].to.val].name);
            }

            if (std::string{ fn->tmp[blk->ins[i].arg[0].val].name } != "") {
                use[std::string{ blk->name }].insert(fn->tmp[blk->ins[i].arg[0].val].name);
            }

            if (std::string{ fn->tmp[blk->ins[i].arg[1].val].name } != "") {
                use[std::string{ blk->name }].insert(fn->tmp[blk->ins[i].arg[1].val].name);
            }
        }
    }

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        std::cout << "@" << blk->name << std::endl;

        std::cout << "\t def = ";
        for (const auto& entry : def[std::string{ blk->name }]) {
            std::cout << "%" << entry << " ";
        }
        std::cout << std::endl;

        std::cout << "\t use = ";
        for (const auto& entry : use[std::string{ blk->name }]) {
            std::cout << "%" << entry << " ";
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