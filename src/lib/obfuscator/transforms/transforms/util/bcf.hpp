#pragma once
#include "analysis/analysis.hpp"
#include "analysis/var_alloc/var_alloc.hpp"
#include "obfuscator/function.hpp"

namespace obfuscator::transform_util {
    /// \brief Generate a new copy of the BB successor and make a [opaque] predicate
    /// \tparam Img X64 or X86 Image
    /// \param function Obfuscator function ptr
    /// \param bb BB ptr
    /// \param post_generation_callback Post generation callback (to tamper instructions or something)
    /// \param predicate_generator Predicate generator
    template <pe::any_image_t Img>
    void generate_bogus_confrol_flow(
        Function<Img>* function, analysis::bb_t* bb,
        const std::function<void(std::shared_ptr<analysis::bb_t>)>& post_generation_callback,
        std::function<void(zasm::x86::Assembler*, zasm::Label, zasm::Label, analysis::VarAlloc<Img>*)> predicate_generator) {

        /// Get the last non-jmp insn
        auto last_insn = bb->last_non_jmp_insn(function->program.get(), true);

        /// Get the successor
        auto successor = last_insn->linear_successor();

        /// Place successor start label
        auto successor_label = function->program->createLabel();
        auto* as = *function->cursor->before(successor->node_at(0));
        as->bind(successor_label);

        /// Crete the label that would be placed at the beginning of the "dead" branch
        auto dummy_bb_label = function->program->createLabel();

        /// Get the var alloc
        auto var_alloc = function->var_alloc();

        /// Temporary disable oserver
        function->observer->stop();

        /// Create dead branch
        as = *function->cursor->after(last_insn->node_ref);
        as->bind(dummy_bb_label);
        auto new_bb = function->bb_storage->copy_bb(successor, as, function->program.get(), function->bb_provider.get());
        new_bb->push_label(as->getCursor(), function->bb_provider.get());

        /// Tamper instructions, if needed
        post_generation_callback(new_bb);

        /// Re-enable observer
        function->observer->start();

        /// Set the cursor, generate predicate
        as = *function->cursor->after(last_insn->node_ref);
        predicate_generator(as, successor_label, dummy_bb_label, &var_alloc);

        /// Update successors, predecessors
        /// \fixme Temporary commented out, uncomment as soon as the fixme in observer is fixed
        // bb->successors.emplace_back(new_bb);
        // new_bb->predecessors.emplace_back(bb);
    }
} // namespace obfuscator::transform_util