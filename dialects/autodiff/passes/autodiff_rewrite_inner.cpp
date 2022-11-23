#include <string>

#include "thorin/tuple.h"

#include "thorin/util/assert.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autogen.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/utils/builder.h"
#include "dialects/autodiff/utils/helper.h"
#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

Lam* AutoDiffEval::create_block(const std::string& name) {
    auto& w  = world();
    auto lam = build(w).mem_ty().mem_cn().with_filter().lam(name);
    push_mem(lam);
    return lam;
}

Lam* AutoDiffEval::create_lam(const Def* cn, const std::string& name) {
    auto& w  = world();
    auto lam = w.nom_lam(cn->as<Pi>(), w.dbg(name));
    lam->set_filter(false);
    lam->set_body(w.bot(w.type_nat()));
    return lam;
}

Lam* AutoDiffEval::push_lam(const Def* cn, const std::string& name) {
    auto lam = create_lam(cn, name);
    push_mem(lam);
    return lam;
}

void AutoDiffEval::ret(Lam* lam) {
    auto& w = world();
    lam->set_body(w.app(lam->ret_var(), pop_mem()));
}

void deep_compare(const Def* src, const Def* dst, std::function<void(const Def*, const Def*)> differ) {
    auto num_projs = src->num_projs();
    assert(num_projs == dst->num_projs());

    if (num_projs > 1) {
        for (size_t i = 0; i < num_projs; i++) { deep_compare(src->proj(i), dst->proj(i), differ); }
    }

    differ(src, dst);
}

const Def* AutoDiffEval::augment_lit(const Lit* lit) { return lit; }

const Def* AutoDiffEval::augment_var(const Var* var) {
    assert(false);
    return var;
}

const Def* AutoDiffEval::augment_lam(Lam* lam) {
    auto& w = world();

    auto lam_ty = lam->type()->as<Pi>();

    size_t needs_context = count_caller(lam) > 1;

    if (needs_context) { lam_ty = w.cn(merge_sigma(lam_ty->dom(), {w.type_int(8)})); }

    auto aug_lam       = push_lam(lam_ty, "aug_" + lam->name());
    const Def* new_var = aug_lam->var();
    if (needs_context) {
        auto projs        = new_var->projs();
        new_var           = w.tuple(projs.skip_back());
        auto branch_index = projs.back();

        lam2branch[lam] = branch_index;
        preserve(branch_index, branch_index);
    }

    augmented[lam->var()] = new_var;
    augmented[lam]        = aug_lam;

    push_scope(lam);
    auto aug_body = augment(lam->body());
    aug_lam->set_body(aug_body);
    pop_scope();
    return aug_lam;
}

const Def* AutoDiffEval::augment_extract(const Extract* ext) {
    auto& world = ext->world();

    auto tuple = ext->tuple();
    auto index = ext->index();

    auto aug_tuple = augment(tuple);
    auto aug_index = augment(index);

    auto aug_ext = world.extract(aug_tuple, aug_index, ext->dbg());

    return aug_ext;
}

const Def* AutoDiffEval::augment_tuple(const Tuple* tup) {
    auto& w = world();
    DefArray aug_ops(tup->ops(), [&](const Def* op) { return augment(op); });
    return w.tuple(aug_ops);
}

const Def* AutoDiffEval::augment_pack(const Pack* pack) {
    auto& w    = world();
    auto shape = augment(pack->shape());
    auto body  = augment(pack->body());
    return w.pack(shape, body);
}

const Def* AutoDiffEval::augment_app(const App* app) {
    auto& w = world();

    auto callee = app->callee();
    auto arg    = app->arg();

    auto aug_arg    = augment(arg);
    auto aug_callee = augment(callee);

    if (auto extract = is_branch(aug_callee)) { add_inverted(callee->as<Extract>()->index(), extract->index()); }

    if (auto callee_lam = callee->isa_nom<Lam>()) {
        size_t needs_context = count_caller(callee_lam) > 1;
        if (needs_context) {
            auto caller      = current_scope();
            const Def* index = branch_id(caller);
            aug_arg          = merge_tuple(aug_arg, {index});
            aug_arg->dump();
        }
    }

    const Def* aug_app;
    if (mem::mem_def(aug_arg)) {
        aug_arg = mem::replace_mem(pop_mem(), aug_arg);
        aug_app = w.app(aug_callee, aug_arg);
        if (auto next_mem = mem::mem_def(aug_app)) { push_mem(next_mem); }
    } else {
        aug_app = w.app(aug_callee, aug_arg);
    }

    return aug_app;
}

const Def* AutoDiffEval::augment_lea(const App* lea) {
    auto& w = world();

    auto [arr_ptr, idx] = lea->arg()->projs<2>();
    auto aug_ptr        = augment(arr_ptr);
    auto aug_idx        = augment(idx);

    auto aug_lea = mem::op_lea(aug_ptr, aug_idx, arr_ptr->dbg());

    return aug_lea;
}

const Def* AutoDiffEval::create_init_frame(const std::string& name, std::function<const Def*(const Def*)> func) {
    auto& w  = world();
    Lam* lam = build(w).mem_ty().lam(name);
    auto mem = func(mem::mem_var(lam));
    init_frames.push(InitFrame{.lam = lam, .mem = mem});
}

const Def* AutoDiffEval::create_init_alloc_frame(const std::string& name, const Def* alloc_ty, bool zero_) {
    const Def* ptr = nullptr;
    create_init_frame("alloc_" + name, [&](const Def* mem) {
        auto& w                                 = mem->world();
        auto alloc_cache                        = mem::op_malloc(alloc_ty, mem, w.dbg(name));
        auto [alloc_cache_mem, alloc_cache_ptr] = alloc_cache->projs<2>();
        ptr                                     = alloc_cache_ptr;

        if (zero_) alloc_cache_mem = mem::op_store(alloc_cache_mem, alloc_cache_ptr, zero(alloc_ty));
        return alloc_cache_mem;
    });

    return ptr;
}

const Def* AutoDiffEval::create_init_slot_frame(const std::string& name, const Def* alloc_ty, bool zero_) {
    const Def* ptr = nullptr;
    create_init_frame("slot_" + name, [&](const Def* mem) {
        auto& w                                 = mem->world();
        auto alloc_cache                        = mem::op_slot(alloc_ty, mem, w.dbg(name));
        auto [alloc_cache_mem, alloc_cache_ptr] = alloc_cache->projs<2>();
        ptr                                     = alloc_cache_ptr;
        if (zero_) alloc_cache_mem = mem::op_store(alloc_cache_mem, alloc_cache_ptr, zero(alloc_ty));
        return alloc_cache_mem;
    });

    return ptr;
}

void AutoDiffEval::preserve(const Def* target, const Def* value) {
    auto& w = world();

    auto ty              = value->type();
    const Def* cache_ptr = nullptr;
    const Def* store_lea = nullptr;
    if (current_loop == root) {
        store_lea = create_init_slot_frame("cache_" + value->name(), ty, true);
        cache_ptr = store_lea;
    } else {
        auto loop_size   = current_loop->size;
        auto cache_index = current_loop->cache_index();

        auto loop_size_nat      = core::op_bitcast(w.type_nat(), loop_size);
        auto arr_ty             = w.arr(loop_size_nat, ty);
        auto alloc_cache_ptr    = create_init_alloc_frame("cache_" + value->name(), arr_ty, true);
        auto unsized_arr_ptr_ty = mem::type_ptr(w.arr(w.top(w.type_nat()), value->type()));
        cache_ptr               = core::op_bitcast(unsized_arr_ptr_ty, alloc_cache_ptr, alloc_cache_ptr->dbg());
        store_lea               = mem::op_lea(cache_ptr, cache_index, w.dbg("lea_cache"));
    }

    op_store(store_lea, value, w.dbg("store_cache"));
    cache_map[target]                = cache_ptr;
    cache_loop_assignment[cache_ptr] = current_loop;
    current_loop->add_cache(cache_ptr, target);
}

const Def* AutoDiffEval::augment_load(const App* load) {
    auto& w = world();

    auto [_, aug_ptr] = augment(load->arg())->projs<2>();

    auto aug_load = op_load_mem(aug_ptr, w.dbg(aug_ptr->name() + "_val"));

    return aug_load;
}

const Def* AutoDiffEval::augment_store(const App* store) {
    auto& world = store->world();

    auto aug_arg               = augment(store->arg());
    auto [_, aug_ptr, aug_val] = aug_arg->projs<3>();
    auto aug_store             = op_store(aug_ptr, aug_val, store->dbg());
    return aug_store;
}

const Def* AutoDiffEval::augment_slot(const App* slot) {
    auto& w = world();
    augment(slot->arg(0_s));
    auto type     = slot->decurry()->arg(0_s);
    auto aug_slot = op_slot_mem(type, slot->dbg());

    auto ptr = slot->proj(1);

    if (has_gradient(ptr)) {
        auto gradient_ptr      = create_init_slot_frame("gradient_" + slot->name(), type, true);
        gradient_pointers[ptr] = gradient_ptr;
    }

    if (current_loop == root) { add_inverted(ptr, aug_slot->proj(1)); }

    return aug_slot;
}

const Def* AutoDiffEval::augment_alloc(const App* alloc) {
    auto& w      = world();
    auto org_ptr = alloc->proj(1);
    augment(alloc->arg(0_s));

    auto type          = alloc->decurry()->arg(0_s);
    auto aug_type      = augment(type);
    auto aug_alloc_mem = op_alloc_mem(aug_type, alloc->dbg());
    auto ptr           = alloc->proj(1);

    if (has_gradient(ptr)) {
        auto gradient_ptr = op_alloc(aug_type, w.dbg("gradient_" + alloc->name()));
        op_store(gradient_ptr, zero(aug_type));
        gradient_pointers[ptr] = gradient_ptr;
    }

    if (current_loop == root) { add_inverted(ptr, aug_alloc_mem->proj(1)); }

    return aug_alloc_mem;
}

const Def* AutoDiffEval::augment_bitcast(const App* bitcast) {
    auto& world = bitcast->world();
    auto arg    = bitcast->arg();

    auto aug_arg = augment(arg);
    auto dst     = core::op_bitcast(bitcast->type(), aug_arg, arg->dbg());

    return dst;
}

const Def* for_size(const Def* start, const Def* end, const Def* inc) { // TODO: support inc larger than 1
    auto& w   = start->world();
    auto size = core::op(core::wrap::sub, core::Mode::none, end, start);
    return size;
}

const Def* normalized_to_loop_index(const Def* start, const Def* inc, const Def* normalized_index) {
    auto correct_increment = core::op(core::wrap::mul, core::Mode::none, normalized_index, inc);
    auto loop_index        = core::op(core::wrap::add, core::Mode::none, correct_increment, start);
    return loop_index;
}

const Def* replace(const Def* tup, int index, const Def* replacement) {
    auto& w = tup->world();
    assert(tup->num_projs() > 1);
    assert(tup->num_projs() >= index + 1);
    auto projs   = tup->projs();
    projs[index] = replacement;
    return w.tuple(projs);
}

const Def* sum(const Def* left, const Def* right) {
    auto& w   = left->world();
    auto test = w.lit_nat(1);
    if (left->isa<Bot>()) { return right; }
    if (right->isa<Bot>()) { return left; }

    if (is_idx(left->type()) && is_idx(right->type())) {
        auto test1 = Idx::size(left->type());
        auto test2 = Idx::size(right->type());
        test1->dump();
        test2->dump();
        auto left_idx = is_idx(left->type());
        assert(*isa_lit(left_idx));
        return core::op(core::wrap::add, core::Mode::none, left, right);
    } else if (match<math::F>(left->type()) && match<math::F>(right->type())) {
        return math::op(math::arith::add, math::Mode::fast, left, right);
    } else if (left->isa<Tuple>()) {
        DefVec new_ops;
        for (size_t i = 0; i < left->num_projs(); i++) { new_ops.push_back(sum(left->proj(i), right->proj(i))); }

        return w.tuple(new_ops);
    }

    assert(false);
}

const Def* AutoDiffEval::resolve(const Def* def) {
    auto test = alias(def);
    if (test && test != def) {
        auto result = resolve(test);

        def->dump();
        test->dump();
        std::cout << "    ";
        result->dump();

        return result;
    }

    if (def->isa<Var>()) {
        def->dump(1);
        def->dump(1);
    }

    if (match<mem::M>(def->type())) { return current_mem; }
    if (def->isa<Axiom>() || def->isa<Lit>()) { return def; }
    const Def* invert;
    if (invert = inverted[def]; !invert) {
        invert        = resolve_impl(def);
        inverted[def] = invert;
    }
    assert(invert);
    return invert;
}

const Def* AutoDiffEval::resolve_impl(const Def* def) {
    auto& w = world();

    assert(!def->isa<Var>());

    if (auto cache_arr = cache_map[def]) {
        // assert(false);
        assert(cache_loop_assignment.contains(cache_arr));
        auto loop = cache_loop_assignment[cache_arr];
        const Def* cache_lea;
        if (loop == root) {
            cache_lea = cache_arr;
        } else {
            auto cache_index = loop->cache_index();
            assert(cache_index);
            cache_lea = mem::op_lea(cache_arr, cache_index);
        }
        return op_load(cache_lea);
    }

    if (auto load = match<mem::load>(def)) {
        auto ptr       = load->arg(1);
        auto cache_ptr = resolve(ptr);
        return op_load_mem(cache_ptr);
    }

    if (auto alloc = match<mem::alloc>(def)) { assert(false); }

    if (auto slot = match<mem::slot>(def)) { assert(false); }

    int need_mem = -1;
    auto new_ops = DefArray(def->num_ops(), [&](auto i) {
        auto op = def->op(i);
        if (match<mem::M>(op)) {
            need_mem = i;
            return (const Def*)nullptr;
        } else {
            return resolve(op);
        }
    });

    if (need_mem != -1) { new_ops[need_mem] = current_mem; }
    auto new_def = def->rebuild(w, def->type(), new_ops, def->dbg());
    if (need_mem != -1) { current_mem = mem::mem_def(new_def); }
    return new_def;
}

void AutoDiffEval::attach_gradient(const Def* dst, const Def* new_gradient) {
    assert(new_gradient);
    assert(dst->type() == new_gradient->type());
    auto gradient = current_loop->gradient[dst];

    const Def* test_grad;
    if (gradient) {
        test_grad = sum(gradient, new_gradient);
    } else {
        test_grad = new_gradient;
    }

    if (dst->type() != test_grad->type()) { sum(gradient, new_gradient); }

    assert(dst->type() == test_grad->type());
    current_loop->gradient[dst] = test_grad;
}

const Def* AutoDiffEval::get_gradient(const Def* def) {
    auto gradient = current_loop->gradient[def];
    if (gradient) {
        gradient->type()->dump();
        def->type()->dump();
        assert(gradient->type() == def->type());
    }
    return gradient;
}

const Def* AutoDiffEval::get_gradient_or_default(const Def* def) {
    if (auto grad = grad_arr(def)) { return grad; }

    auto result = get_gradient(def);
    if (result == nullptr) {
        if (def->num_projs() > 1) {
            DefVec grads;
            for (auto proj : def->projs()) { grads.push_back(get_gradient_or_default(proj)); }
            return world().tuple(grads);
        } else {
            return zero(def->type());
        }
    }

    return result;
}

void AutoDiffEval::check_grad_arr(const Def* def) {
    auto& w = world();

    if (auto lea = match<mem::lea>(def)) {
        auto [arr, index] = lea->args<2>();
        check_grad_arr(arr);
    } else if (auto bitcast = match<core::bitcast>(def)) {
        check_grad_arr(bitcast->arg());
    } else {
        if (auto grad_ptr = gradient_pointers[def]) {
        } else if (auto extract = def->isa<Extract>()) {
            auto tup = extract->tuple();
            if (auto alloc = match<mem::alloc>(tup)) {
                assert(false);
                auto ptr          = alloc->proj(1);
                auto type         = alloc->decurry()->arg(0_s);
                auto aug_type     = augment(type);
                auto gradient_ptr = op_alloc(aug_type, w.dbg("gradient_arr_" + def->name()));
                op_store(gradient_ptr, zero(aug_type));
                current_loop->allocated_memory.insert(gradient_ptr);
                gradient_pointers[ptr] = gradient_ptr;
            } else if (auto slot = match<mem::slot>(tup)) {
                assert(false);
                auto ptr          = slot->proj(1);
                auto type         = slot->decurry()->arg(0_s);
                auto gradient_ptr = op_slot(type, w.dbg("gradient_slot_" + def->name()));
                op_store(gradient_ptr, zero(type));
                gradient_pointers[ptr] = gradient_ptr;
            }
        }
    }
}

const Def* AutoDiffEval::grad_arr(const Def* def) {
    auto& w = world();

    auto test = alias(def);
    if (test) { def = test; }

    if (auto grad_ptr = gradient_pointers[def]) {
        return grad_ptr;
    } else if (auto lea = match<mem::lea>(def)) {
        auto [arr, index] = lea->args<2>();
        auto grad         = grad_arr(arr);
        if (grad != nullptr) return mem::op_lea(grad, resolve(index));
    } else if (auto bitcast = match<core::bitcast>(def)) {
        auto grad = grad_arr(bitcast->arg());
        if (grad != nullptr) return w.app(bitcast->callee(), grad);
    }

    return nullptr;
}

const Def* scale(const Def* shape, const Def* def) {
    auto& w = shape->world();
    if (def->num_projs() > 1) {
        DefArray new_ops(def->projs(), [&](const Def* op) { return scale(shape, op); });
        return w.tuple(new_ops);
    }

    if (match<math::F>(def->type())) {
        auto shape_idx       = core::op_bitcast(w.type_int(64), shape);
        auto shape_val       = math::op(math::conv::u2f, def->type(), shape_idx);
        auto scaled_gradient = math::op(math::arith::mul, math::Mode::fast, shape_val, def);
        return scaled_gradient;
    } else if (is_idx(def->type())) {
        auto shape_idx       = core::op_bitcast(def->type(), shape);
        auto scaled_gradient = core::op(core::wrap::mul, core::Mode::none, shape_idx, def);
        return scaled_gradient;
    }

    assert(false);
}

void AutoDiffEval::prop(const Def* def) {
    if (def->type()->isa<Pi>()) return;
    if (!visited_prop.insert(def).second) { return; }
    auto& w = world();

    /*if (def->isa<Var>()) {
        DefVec result;
        for( auto var : def->projs() ){
            result.push_back(get_gradient(var, var->type()));
        }
        attach_gradient(def, w.tuple(result));
        return;
    }*/

    if (auto store = match<mem::store>(def)) {
        auto ptr      = store->arg(1);
        auto grad_ptr = grad_arr(ptr);

        if (grad_ptr) {
            auto load_val = op_load(grad_ptr);
            op_store(grad_ptr, zero(load_val->type()));
            attach_gradient(store->arg(), one_hot_other_default(store->arg(), load_val, 2));
        }
        return;
    }

    if (auto alloc = match<mem::alloc>(def)) {
        auto ptr      = alloc->proj(1);
        auto grad_ptr = grad_arr(ptr);

        if (grad_ptr) { op_free(grad_ptr, w.dbg("free_" + grad_ptr->name())); }
        return;
    }

    auto gradient = get_gradient(def);
    if (gradient == nullptr) { return; }

    if (auto load = match<mem::load>(def)) {
        auto arr  = load->arg(1);
        auto val  = load->proj(1);
        auto grad = grad_arr(arr);
        if (grad) {
            assert(grad);
            auto gradient_val = gradient->proj(1);
            assert(gradient_val);

            auto load_val     = op_load(grad);
            auto get_gradient = sum(load_val, gradient_val);
            op_store(grad, get_gradient);
        }
        return;
    }

    if (auto tuple = def->isa<Tuple>()) {
        auto ops  = tuple->ops();
        auto ops2 = gradient->ops();
        for (size_t i = 0; i < tuple->num_ops(); i++) { attach_gradient(tuple->op(i), gradient->proj(i)); }
        return;
    }

    if (auto extract = def->isa<Extract>()) {
        auto tup   = extract->tuple();
        auto index = as_lit(extract->index());

        attach_gradient(tup, one_hot_other_default(tup, gradient, index));
        return;
    }

    if (auto pack = def->isa<Pack>()) {
        auto shape = resolve(pack->shape());
        auto body  = pack->body();

        auto gradient_val = gradient->as<Pack>()->body();
        attach_gradient(body, scale(shape, gradient_val));
        return;
    }

    if (auto exp = match<math::exp>(def)) {
        const Def* upstream_grad;
        if (exp.id() == math::exp::exp) {
            auto result_exp = resolve(exp);
            upstream_grad   = math::op(math::arith::mul, math::Mode::fast, result_exp, gradient);
        } else if (exp.id() == math::exp::log) {
            auto result_arg = resolve(exp->arg());
            upstream_grad   = math::op(math::arith::div, math::Mode::fast, gradient, result_arg);
        }
        attach_gradient(exp->arg(), upstream_grad);
    }

    if (auto tri = match<math::tri>(def)) {
        auto result_arg = resolve(tri->arg());

        const Def* local_grad = nullptr;
        if (tri.id() == math::tri::tanh) {
            auto cosh          = math::op(math::tri::cosh, math::Mode::fast, result_arg);
            auto cosh2         = math::op(math::arith::mul, math::Mode::fast, cosh, cosh);
            auto upstream_grad = math::op(math::arith::div, math::Mode::fast, gradient, cosh2);
            attach_gradient(tri->arg(), upstream_grad);
            return;
        } else if (tri.id() == math::tri::sin) {
            local_grad = math::op(math::tri::cos, math::Mode::fast, result_arg);
        } else if (tri.id() == math::tri::cos) {
            local_grad = math::op(math::tri::sin, math::Mode::fast, result_arg);
            local_grad = math::op_rminus(math::Mode::fast, local_grad);
        }

        assert(local_grad);
        auto upstream_grad = math::op(math::arith::mul, math::Mode::fast, gradient, local_grad);
        attach_gradient(tri->arg(), upstream_grad);
    }

    if (auto rt = match<math::rt>(def)) {
        auto rt_val = resolve(rt);
        auto s      = math::isa_f(rt_val->type());
        assert(s);
        auto two           = math::lit_f(w, *s, 2.0);
        auto local_grad    = math::op(math::arith::mul, math::Mode::fast, two, rt_val);
        auto upstream_grad = math::op(math::arith::div, math::Mode::fast, gradient, local_grad);
        attach_gradient(rt->arg(), upstream_grad);
    }

    if (auto gamma = match<math::gamma>(def)) {
        auto x = resolve(gamma->arg());
        auto s = math::isa_f(x->type());
        assert(s);
        auto delta     = math::lit_f(w, *s, 0.0000001);
        auto two       = math::lit_f(w, *s, 2.0);
        auto two_delta = math::op(math::arith::mul, math::Mode::fast, two, delta);

        auto left_x = math::op(math::arith::add, math::Mode::fast, x, delta);
        auto left   = math::op(math::gamma::l, math::Mode::fast, left_x);

        auto right_x = math::op(math::arith::sub, math::Mode::fast, x, delta);
        auto right   = math::op(math::gamma::l, math::Mode::fast, right_x);

        auto sum  = math::op(math::arith::sub, math::Mode::fast, left, right);
        auto grad = math::op(math::arith::div, math::Mode::fast, sum, two_delta);
        attach_gradient(gamma->arg(), grad);
    }

    if (auto extrema = match<math::extrema>(def)) {
        if (extrema.id() == math::extrema::maximum) {
            auto [left, right] = extrema->args<2>();
            auto left_value    = resolve(left);
            auto right_value   = resolve(right);

            auto comparison     = math::op(math::cmp::g, w.lit_nat(math::Mode::fast), left_value, right_value);
            auto zero_val       = zero(gradient->type());
            auto left_gradient  = w.tuple({zero_val, gradient});
            auto right_gradient = w.tuple({gradient, zero_val});

            auto first_select  = w.extract(left_gradient, comparison);
            auto second_select = w.extract(right_gradient, comparison);

            auto result = w.tuple({first_select, second_select});
            attach_gradient(extrema->arg(), result);
            return;
        }
    }

    if (auto wrap = match<core::wrap>(def)) {
        auto [left, right] = wrap->args<2>();

        const Def* left_grad  = gradient;
        const Def* right_grad = gradient;

        if (wrap.id() == core::wrap::add) {
        } else if (wrap.id() == core::wrap::sub) {
            right_grad = core::op_wminus(core::Mode::none, gradient);
        } else if (wrap.id() == core::wrap::mul) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            left_grad  = core::op(core::wrap::mul, core::Mode::none, gradient, right_value);
            right_grad = core::op(core::wrap::mul, core::Mode::none, gradient, left_value);
        }

        attach_gradient(wrap->arg(), w.tuple({left_grad, right_grad}));
    } else if (auto div = match<core::div>(def)) {
        auto [mem, left, right] = div->args<3>();
        if (div.id() == core::div::sdiv || div.id() == core::div::udiv) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            auto [div_mem, left_grad] = core::op(div.id(), current_mem, gradient->proj(1), right_value)->projs<2>();
            auto right_grad =
                core::op(core::wrap::mul, core::Mode::none, core::op_wminus(core::Mode::none, left_grad), left_value);
            current_mem = div_mem;
            attach_gradient(div->arg(), w.tuple({w.bot(mem::type_mem(w)), left_grad, right_grad}));
        }
    } else if (auto rop = match<math::arith>(def)) {
        auto [left, right] = rop->args<2>();

        const Def* left_grad  = gradient;
        const Def* right_grad = gradient;

        if (rop.id() == math::arith::add) {
        } else if (rop.id() == math::arith::sub) {
            right_grad = math::op_rminus(math::Mode::fast, gradient);
        } else if (rop.id() == math::arith::mul) {
            auto left_value = resolve(left);
            right_grad      = math::op(math::arith::mul, math::Mode::fast, gradient, left_value);

            auto right_value = resolve(right);
            left_grad        = math::op(math::arith::mul, math::Mode::fast, gradient, right_value);
        } else if (rop.id() == math::arith::div) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);
            left_grad        = math::op(math::arith::div, math::Mode::fast, gradient, right_value);

            right_grad  = math::op_rminus(math::Mode::fast, gradient);
            right_grad  = math::op(math::arith::mul, math::Mode::fast, right_grad, left_value);
            right_value = math::op(math::arith::mul, math::Mode::fast, right_value, right_value);
            right_grad  = math::op(math::arith::div, math::Mode::fast, right_grad, right_value);
        }

        attach_gradient(rop->arg(), w.tuple({left_grad, right_grad}));
    }
}

const Def* zero_def(const Def* mem, const Def* ty) {
    auto& w = ty->world();
    if (auto sig = ty->isa<Sigma>()) {
        DefArray ops(sig->ops(), [&](const Def* op) { return zero_def(mem, op); });
        return w.tuple(ops);
    } else if (match<mem::M>(ty)) {
        return mem;
    } else {
        return zero(ty);
    }
}

const Def* rm_cns(const Def* def) {
    auto& w = def->world();
    if (def->num_projs() > 1) {
        DefVec vec;
        for (auto proj : def->projs()) {
            auto op = rm_cns(proj);
            if (op != w.tuple()) { vec.push_back(op); }
        }
        return w.tuple(vec);
    }

    return def->type()->isa<Pi>() ? w.tuple() : def;
}

Lam* AutoDiffEval::invert_lam(AffineCFNode* node, const Def* ret_var) {
    if (auto inv = lam2inv[node->def()]) { return inv; }
    auto& w = world();
    assert(node);

    auto caller_lam = node->def()->as_nom<Lam>();
    auto app        = caller_lam->body()->as<App>();

    const Def* arg = thorin::autodiff::arg(app);

    auto current_lam = push_lam(w.cn(arg->type()), "invert_" + caller_lam->name());
    auto inv_arg     = current_lam->arg();

    for (auto load : requires_loading(caller_lam)) { resolve(load); }

    if (auto branch_index = lam2branch[caller_lam]) { resolve(branch_index); }

    lam2inv[node->def()] = current_lam;

    auto return_lam = current_lam;
    if (match<affine::For>(app)) {
        auto [start, end, inc, acc, body, exit] = app->args<6>();

        auto aug_start = resolve(start);
        auto aug_end   = resolve(end);
        auto aug_inc   = resolve(inc);

        auto invert_start = zero(w);
        auto invert_inc   = one(w);

        auto size       = for_size(aug_start, aug_end, aug_inc);
        auto invert_end = size;

        auto body_lam = body->as_nom<Lam>();

        auto invert_for_mem = pop_mem();

        push_loop_frame(app, size);

        auto loop_body = body->isa_nom<Lam>();
        assert(loop_body);

        auto raw_index = loop_body->arg(0_s);

        Lam* inv_loop_body = push_lam(loop_body->type()->as<Pi>(), "invert_body_" + loop_body->name());

        const Def* relative_size = core::op(core::wrap::sub, core::Mode::none, aug_end, aug_start);
        relative_size            = core::op(core::wrap::sub, core::Mode::none, relative_size, one(w));

        auto normalized_index = inv_loop_body->arg(0_s);
        normalized_index      = core::op(core::wrap::sub, core::Mode::none, relative_size, normalized_index);

        current_loop->index()       = normalized_to_loop_index(aug_start, aug_inc, normalized_index);
        current_loop->cache_index() = normalized_to_cache_index(normalized_index);

        add_inverted(raw_index, current_loop->index());

        auto loop_exit_node = factory->cfa().node(body_lam->ret_var());
        auto exit_lam       = loop_exit_node->pred();

        for (auto& [cache, targets] : current_loop->caches()) {
            auto cache_index = current_loop->cache_index();
            assert(cache_index);
            auto cache_ptr = mem::op_lea(cache, cache_index);
            auto load_val  = op_load(cache_ptr);

            for (auto target : targets) { add_inverted(target, load_val); }
        }

        auto inv_loop_body_mem = pop_mem();
        auto inv_body_lam_new  = invert_lam(exit_lam, inv_loop_body->ret_var());

        pop_loop_frame();

        inv_loop_body->set_body(w.app(inv_body_lam_new, mem::replace_mem(inv_loop_body_mem, inv_loop_body->var(1))));

        const Def* gradient = mem::replace_mem(invert_for_mem, current_lam->arg());

        Lam* invert_exit = push_lam(exit->type()->as<Pi>(), "invert_exit_" + current_lam->name());

        current_lam->set_body(
            affine::op_for(w, invert_start, invert_end, invert_inc, gradient->projs(), inv_loop_body, invert_exit));

        attach_gradient(acc, invert_exit->var());
        current_lam = invert_exit;
    } else {
        auto num_proj = arg->num_projs();
        assert(num_proj == arg->num_projs());
        attach_gradient(arg, inv_arg);
    }

    for (auto node : factory->dfa().post_order(caller_lam)) { prop(node->def()); }

    auto gradient = get_gradient_or_default(caller_lam->arg());

    const Def* result_callee = nullptr;
    if (!caller_lam->ret_var()) {
        auto callers = node->preds();
        DefVec branches;

        for (auto caller : callers) {
            if (!caller->def()->isa_nom<Lam>()) continue;
            auto inv_lam = invert_lam(caller, ret_var);
            branches.push_back(inv_lam);
        }

        if (branches.size() == 0) {
            assert(false);
        } else if (branches.size() == 1) {
            result_callee = branches[0];
        } else {
            auto branch_idx = resolve(lam2branch[caller_lam]);

            DefArray arr(branches.size());

            size_t i = 0;
            for (auto caller : callers) {
                if (!caller->def()->isa_nom<Lam>()) continue;
                auto branch_id = branch_id_lit(caller->def());
                arr[branch_id] = branches[i++];
            }

            auto branch_tup = w.tuple(arr);
            auto conv_idx   = core::op_bitcast(w.type_idx(branches.size()), branch_idx);
            auto extract    = w.extract(branch_tup, conv_idx);
            result_callee   = extract;
        }
    } else {
        if (current_loop != root) { gradient = gradient->proj(1); }
        result_callee = ret_var;
    }

    gradient = mem::replace_mem(pop_mem(), gradient);
    current_lam->set_body(w.app(result_callee, gradient));
    return return_lam;
}

Lam* AutoDiffEval::invert_lam(Lam* lam) {
    auto& w = world();

    if (lam->is_returning()) {
        auto ret_dom    = lam->ret_dom();
        auto arg_type   = lam->arg()->type();
        auto inv_arg_ty = w.sigma({ret_dom, w.cn(arg_type)});
        auto inv_pi     = w.cn(flatten_deep(inv_arg_ty));
        auto inv_lam    = create_lam(inv_pi, "inv_func_" + lam->name());

        auto& cfa = factory->cfa();
        auto exit = cfa.node(lam->ret_var());

        auto ret_wrap = exit->pred();

        Lam* lam = invert_lam(ret_wrap, inv_lam->ret_var());
        inv_lam->set_body(w.app(lam, inv_lam->arg()));

        return inv_lam;
    }

    assert(false);
}

const Def* AutoDiffEval::normalized_to_cache_index(const Def* normalized_index) {
    assert(normalized_index);
    const Def* cache_index;
    auto parent = current_loop->parent;
    if (parent != nullptr) {
        const Def* offset =
            core::op(core::wrap::mul, core::Mode::none, current_loop->local_size, parent->cache_index());
        normalized_index = core::op_bitcast(offset->type(), normalized_index);
        return core::op(core::wrap::add, core::Mode::none, offset, normalized_index);
    } else {
        return normalized_index;
    }
}

const Def* AutoDiffEval::rewrite_rebuild(Rewriter& rewriter, const Def* def) {
    auto result = rewriter.rewrite(def);

    if (result != def) { return result; }

    if (def->isa<Var>() || def->isa<Axiom>()) { return def; }

    auto& w      = world();
    auto new_ops = DefArray(def->num_ops(), [&](auto i) { return rewrite_rebuild(rewriter, def->op(i)); });
    auto new_def = def->rebuild(w, def->type(), new_ops, def->dbg());
    return new_def;
}

const Def* AutoDiffEval::invert_for(const App* for_app) {
    auto& w = world();
    return nullptr;
}

const Def* AutoDiffEval::upper_bound_size(const Def* size, bool lower) {
    if (auto loop = index_loop_map[size]) {
        if (lower) {
            return zero(size->type());
        } else {
            return loop->local_size;
        }
    }

    auto& w = world();
    if (auto wrap = match<core::wrap>(size)) {
        auto [left, right] = wrap->args<2>();

        auto new_left  = upper_bound_size(left, lower);
        auto new_right = upper_bound_size(right, lower != (wrap.id() == core::wrap::sub));

        return w.app(wrap->callee(), {new_left, new_right});
    }

    return size;
}

void AutoDiffEval::push_loop_frame(const App* for_app, const Def* size) {
    if (auto cached_loop = loop_assignment[for_app]) {
        current_loop = cached_loop;
    } else {
        auto next_frame          = std::make_shared<LoopFrame>(*this);
        next_frame->parent       = current_loop;
        next_frame->for_def      = for_app;
        size                     = upper_bound_size(size);
        auto loop_index_ty       = current_loop->size->type();
        size                     = core::op_bitcast(loop_index_ty, size);
        next_frame->size         = core::op(core::wrap::mul, core::Mode::none, current_loop->size, size);
        next_frame->local_size   = size;
        loop_assignment[for_app] = next_frame;
        current_loop             = next_frame;
    }
}

void AutoDiffEval::pop_loop_frame() {
    assert(current_loop->parent);
    current_loop = current_loop->parent;
}

const Def* AutoDiffEval::augment_for_body(const Def* body, const Def* start, const Def* inc) {
    auto& w = world();

    auto loop_body = body->as_nom<Lam>();
    check_mem();
    Lam* aug_body = push_lam(loop_body->type()->as<Pi>(), "aug_" + loop_body->name());

    auto org_index              = loop_body->var(0_s);
    auto normalized_index       = aug_body->var(0_s);
    current_loop->index()       = normalized_to_loop_index(start, inc, normalized_index);
    current_loop->cache_index() = normalized_to_cache_index(normalized_index);

    index_loop_map[normalized_index] = current_loop;

    augmented[org_index]        = current_loop->index();
    augmented[loop_body->var()] = replace(aug_body->var(), 0, current_loop->index());
    augmented[loop_body]        = aug_body;

    aug_body->set_body(augment(loop_body->body()));
    check_mem();
    return aug_body;
}

const Def* AutoDiffEval::augment_for(const App* for_app) {
    auto& w                                 = world();
    auto [start, end, inc, acc, body, exit] = for_app->args<6>();

    auto aug_start = augment(start);
    auto aug_end   = augment(end);
    auto aug_inc   = augment(inc);
    auto aug_acc   = augment(acc);

    auto size = for_size(aug_start, aug_end, aug_inc);

    auto body_lam = body->as_nom<Lam>();

    Scope scope(body_lam);
    for (const Def* free_def : scope.free_defs()) {
        auto free_ty = free_def->type();
        if (is_idx(free_ty) || match<math::F>(free_ty)) { augment(free_def); }
    }
    push_loop_frame(for_app, size);
    auto aug_body = augment_for_body(body, aug_start, aug_inc);
    pop_loop_frame();
    auto aug_exit = augment(exit);
    check_mem();

    aug_acc = mem::replace_mem(pop_mem(), aug_acc);
    return affine::op_for(w, zero(w), size, one(w), aug_acc->projs(), aug_body, aug_exit);
}

const Def* AutoDiffEval::augment_(const Def* def) {
    auto& w = world();

    w.DLOG("Augment def {} : {}", def, def->type());
    if (auto for_app = match<affine::For>(def)) { return augment_for(for_app); }

    if (auto lea = match<mem::lea>(def)) { return augment_lea(lea->as<App>()); }

    if (auto load = match<mem::load>(def)) { return augment_load(load->as<App>()); }

    if (auto store = match<mem::store>(def)) { return augment_store(store->as<App>()); }

    if (auto slot = match<mem::slot>(def)) { return augment_slot(slot->as<App>()); }

    if (auto alloc = match<mem::alloc>(def)) { return augment_alloc(alloc->as<App>()); }

    if (auto bitcast = match<core::bitcast>(def)) { return augment_bitcast(bitcast->as<App>()); }

    if (auto arr = def->isa<Arr>()) {
        auto shape = augment(arr->shape());
        auto body  = augment(arr->body());
        return w.arr(shape, body);
    }

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        return augment_app(app);
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
        auto tuple = ext->tuple();
        auto index = ext->index();
        w.DLOG("Augment extract: {} #[{}]", tuple, index);
        return augment_extract(ext);
    }

    // vars (function argument)
    else if (auto var = def->isa<Var>()) {
        w.DLOG("Augment variable: {}", var);
        return augment_var(var);
    }

    // lam
    else if (auto lam = def->isa_nom<Lam>()) {
        w.DLOG("Augment nom lambda: {}", lam);
        return augment_lam(lam);
    } else if (auto lam = def->isa<Lam>()) {
        w.ELOG("Augment lambda: {}", lam);
        assert(false && "can not handle non-nominal lambdas");
    }

    // constants
    else if (auto lit = def->isa<Lit>()) {
        w.DLOG("Augment literal: {}", def);
        return augment_lit(lit);
    }

    // tuple
    else if (auto tup = def->isa<Tuple>()) {
        w.DLOG("Augment tuple: {}", def);
        return augment_tuple(tup);
    }

    // pack
    else if (auto pack = def->isa<Pack>()) {
        // TODO: nom pack
        auto shape = pack->arity(); // TODO: arity vs shape
        auto body  = pack->body();
        w.DLOG("Augment pack: {} : {} with {}", shape, shape->type(), body);
        return augment_pack(pack);
    }

    return def;
}

const Def* AutoDiffEval::invert_(const Def* def) {
    auto& w = world();

    w.DLOG("Augment def {} : {}", def, def->type());

    if (auto for_app = match<affine::For>(def)) { return invert_for(for_app); }

    if (auto lea = match<mem::lea>(def)) { return invert_lea(lea->as<App>()); }

    if (auto load = match<mem::load>(def)) { return invert_load(load->as<App>()); }

    if (auto store = match<mem::store>(def)) { return invert_store(store->as<App>()); }

    if (auto malloc = match<mem::malloc>(def)) { return invert_malloc(malloc->as<App>()); }

    if (auto alloc = match<mem::alloc>(def)) { return invert_alloc(alloc->as<App>()); }

    if (auto bitcast = match<core::bitcast>(def)) { return invert_bitcast(bitcast->as<App>()); }

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg    = app->arg();
        w.DLOG("Augment application: app {} with {}", callee, arg);
        return invert_app(app);
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
        // world.DLOG("Augment extract: {}", def);
        auto tuple = ext->tuple();
        auto index = ext->index();
        w.DLOG("Augment extract: {} #[{}]", tuple, index);
        return invert_extract(ext);
    }

    // vars (function argument)
    else if (auto var = def->isa<Var>()) {
        w.DLOG("Augment variable: {}", var);
        return invert_var(var);
    }

    // lam
    else if (auto lam = def->isa_nom<Lam>()) {
        w.DLOG("Augment nom lambda: {}", lam);
        return invert_lam(lam);
    } else if (auto lam = def->isa<Lam>()) {
        w.ELOG("Augment lambda: {}", lam);
        assert(false && "can not handle non-nominal lambdas");
    }

    // constants
    else if (auto lit = def->isa<Lit>()) {
        w.DLOG("Augment literal: {}", def);
        return invert_lit(lit);
    }

    // tuple
    else if (auto tup = def->isa<Tuple>()) {
        w.DLOG("Augment tuple: {}", def);
        return invert_tuple(tup);
    } else if (auto ax = def->isa<Axiom>()) {
        return ax;
    }
}

const Def* AutoDiffEval::invert_var(const Var* var) {}
const Def* AutoDiffEval::invert_extract(const Extract* extract) {}
const Def* AutoDiffEval::invert_app(const App* app) {}
const Def* AutoDiffEval::invert_lit(const Lit* lit) {}
const Def* AutoDiffEval::invert_tuple(const Tuple* tuple) { return tuple; }
const Def* AutoDiffEval::invert_pack(const Pack* pack) { return pack; }
const Def* AutoDiffEval::invert_lea(const App* lea) {}
const Def* AutoDiffEval::invert_store(const App* store) {}
const Def* AutoDiffEval::invert_malloc(const App* malloc) {}
const Def* AutoDiffEval::invert_alloc(const App* alloc) {}
const Def* AutoDiffEval::invert_bitcast(const App* bitcast) {}
const Def* AutoDiffEval::invert_load(const App* load) { assert(false); }

const Def* AutoDiffEval::op_load_mem(const Def* ptr, const Def* dbg) {
    check_mem();
    auto load   = mem::op_load(current_mem, ptr, dbg);
    current_mem = load->proj(0);
    return load;
}

const Def* AutoDiffEval::op_load(const Def* ptr, const Def* dbg) { return op_load_mem(ptr, dbg)->proj(1); }

const Def* AutoDiffEval::op_store(const Def* ptr, const Def* value, const Def* dbg) {
    check_mem();
    current_mem = mem::op_store(current_mem, ptr, value, dbg);
    return current_mem;
}

const Def* AutoDiffEval::op_free(const Def* ptr, const Def* dbg) {
    check_mem();
    current_mem = mem::op_free(current_mem, ptr, dbg);
    return current_mem;
}

const Def* AutoDiffEval::op_slot_mem(const Def* type, const Def* dbg) {
    check_mem();
    auto& w     = world();
    auto slot   = mem::op_slot(type, current_mem, dbg);
    current_mem = slot->proj(0);
    return slot;
}

const Def* AutoDiffEval::op_slot(const Def* type, const Def* dbg) { return op_slot_mem(type, dbg)->proj(1); }

const Def* AutoDiffEval::op_alloc_mem(const Def* type, const Def* dbg) {
    check_mem();
    auto& w     = world();
    auto slot   = mem::op_alloc(type, current_mem, dbg);
    current_mem = slot->proj(0);
    return slot;
}

const Def* AutoDiffEval::op_alloc(const Def* type, const Def* dbg) { return op_alloc_mem(type, dbg)->proj(1); }

} // namespace thorin::autodiff