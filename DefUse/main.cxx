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
    Blk* last_blk = nullptr;

    for (Blk* blk = fn->start; blk; blk = blk->link) {
        if (blk->jmp.type == Jret0 || blk->jmp.type >= Jretw) {
            last_blk = blk;
        }

        for (int i = 0; i < blk->nins; i++) {
            std::string arg1 = fn->tmp[blk->ins[i].arg[0].val].name;
            std::string arg2 = fn->tmp[blk->ins[i].arg[1].val].name;

            if (std::string{ arg1 } != "" && !def[std::string{ blk->name }].count(arg1)) {
                use[std::string{ blk->name }].insert(arg1);
            }

            if (std::string{ arg2 } != "" && !def[std::string{ blk->name }].count(arg2)) {
                use[std::string{ blk->name }].insert(arg2);
            }

            if (std::string{ fn->tmp[blk->ins[i].to.val].name } != "") {
                def[std::string{ blk->name }].insert(fn->tmp[blk->ins[i].to.val].name);
            }
        }
    }

    std::string ret_var = fn->tmp[last_blk->jmp.arg.val].name;

    if (std::string{ ret_var } != "" && !def[std::string{ last_blk->name }].count(ret_var)) {
        use[std::string{ last_blk->name }].insert(ret_var);
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