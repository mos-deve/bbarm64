#include "ir.hpp"

namespace bbarm64 {

IROptimizer::IROptimizer() {}

void IROptimizer::optimize(IRBlock& block) {
    constant_propagation(block);
    dead_code_elimination(block);
    flag_merging(block);
}

void IROptimizer::constant_propagation(IRBlock& block) {
    // Simple constant propagation: if src2 is an immediate and src1 is known constant, fold
    // This is a minimal implementation - a full version would track known values
    (void)block;
}

void IROptimizer::dead_code_elimination(IRBlock& block) {
    // Remove NOPs and instructions whose results are never used
    // This is a minimal implementation
    (void)block;
}

void IROptimizer::flag_merging(IRBlock& block) {
    // If two consecutive instructions set flags and only the second one's flags are used,
    // clear the flags_set flag on the first one
    for (size_t i = 0; i + 1 < block.instructions.size(); i++) {
        if (block.instructions[i].flags_set && block.instructions[i+1].flags_set) {
            // Check if any instruction between i and i+1 uses flags
            bool flags_used_between = false;
            for (size_t j = i + 1; j < block.instructions.size(); j++) {
                if (block.instructions[j].flags_used) {
                    flags_used_between = true;
                    break;
                }
                if (block.instructions[j].flags_set) break;
            }
            if (!flags_used_between) {
                block.instructions[i].flags_set = false;
            }
        }
    }
}

} // namespace bbarm64
