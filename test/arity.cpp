#include "gtest/gtest.h"

#include "thorin/world.h"
#include "thorin/fe/parser.h"

using namespace thorin;
using namespace thorin::fe;

TEST(Arity, Successor) {
    World w;
    auto a0 = w.lit_arity(0);
    EXPECT_EQ(w.lit_arity(1), w.arity_succ(a0));
    EXPECT_EQ(w.lit_arity(42), w.arity_succ(w.lit_arity(41)));
    EXPECT_TRUE(w.arity_succ(w.var(w.kind_arity(), 0))->isa<App>());
}

TEST(Arity, Sigma) {
    World w;
    auto a0 = w.lit_arity(0);
    auto a1 = w.lit_arity(1);
    auto a2 = w.lit_arity(2);
    auto a3 = w.lit_arity(3);

    EXPECT_EQ(a0, w.sigma({a0, a0}));
    EXPECT_EQ(a0, w.sigma({a1, a0}));
    EXPECT_EQ(a0, w.sigma({a0, a1}));

    EXPECT_EQ(w.kind_multi(), w.sigma({a2, a3})->type());
    EXPECT_EQ(w.kind_star (), w.sigma({a2, w.sigma({a3, a2}), a3})->type());
}

TEST(Arity, DependentSigma) {
    World w;
    // auto a0 = w.lit_arity(0);
    // auto a1 = w.lit_arity(1);
    auto a2 = w.lit_arity(2);
    auto a3 = w.lit_arity(3);

    auto arity_dep = w.sigma({a2, w.extract(w.tuple({a3, a2}), w.var(a2, 0))});
    arity_dep->dump();
    arity_dep->type()->dump();
    // TODO actual test on arity_dep
}

TEST(Arity, Variadic) {
    World w;
    auto a0 = w.lit_arity(0);
    auto a1 = w.lit_arity(1);
    auto a2 = w.lit_arity(2);
    auto unit = w.unit();

    EXPECT_EQ(unit, w.variadic({a0, a1, a2}, unit));
    EXPECT_EQ(unit, w.variadic({a1, a0}, unit));
    EXPECT_EQ(w.unit_kind(), w.variadic({a0, a2}, w.kind_arity()));
    EXPECT_EQ(w.variadic(a2, w.unit_kind()), w.variadic({a2, a0, a2}, w.kind_arity()));
    EXPECT_EQ(w.kind_multi(), w.variadic(a2, a2)->type());
}

TEST(Arity, Pack) {
    World w;
    auto a0 = w.lit_arity(0);
    auto a1 = w.lit_arity(1);
    auto a2 = w.lit_arity(2);
    auto unit = w.unit();
    auto tuple0t = w.val_unit_kind();

    EXPECT_EQ(tuple0t, w.pack({a0, a1, a2}, unit));
    EXPECT_EQ(tuple0t, w.pack({a1, a0}, unit));
    EXPECT_EQ(w.pack(a2, tuple0t), w.pack({a2, a0, a2}, unit));
}

TEST(Arity, Subkinding) {
    World w;
    auto a0 = w.lit_arity(0);
    auto a3 = w.lit_arity(3);
    auto A = w.kind_arity();
    auto vA = w.var(A, 0);
    auto M = w.kind_multi();

    EXPECT_TRUE(A->assignable(a0));
    EXPECT_TRUE(M->assignable(a0));
    EXPECT_TRUE(A->assignable(a3));
    EXPECT_TRUE(M->assignable(a3));
    EXPECT_TRUE(A->assignable(vA));
    EXPECT_TRUE(M->assignable(vA));

    auto sig = w.sigma({M, w.variadic(w.var(M, 0), w.kind_star())});
    EXPECT_TRUE(sig->assignable(w.tuple({a0, w.val_unit_kind()})));
}

TEST(Arity, PrefixExtract) {
    World w;
    auto a2 = w.lit_arity(2);
    auto i1_2 = w.lit_index(2, 1);
    auto a3 = w.lit_arity(3);
    auto i2_3 = w.lit_index(3, 2);

    auto unit = w.unit();
    auto ma = w.sigma({a2, a3, a2});
    auto marray = w.variadic(ma, unit);
    auto v_marray = w.var(marray, 0);

    EXPECT_EQ(unit, w.extract(v_marray, w.tuple({i1_2, i2_3, i1_2}))->type());
    EXPECT_EQ(w.variadic(a2, unit), w.extract(v_marray, w.tuple({i1_2, i2_3}))->type());
    EXPECT_EQ(w.variadic(a3, w.variadic(a2, unit)), w.extract(v_marray, i1_2)->type());
    EXPECT_EQ(w.variadic(w.sigma({a3, a2}), unit), w.extract(v_marray, i1_2)->type());
}

TEST(Arity, DependentArityEliminator) {
    World w;
    auto lam_bool = w.lambda(w.kind_arity(), w.type_bool());
    auto step_negate = w.lambda(w.kind_arity(), w.lambda(w.type_bool(), w.op_bnot(w.var(w.type_bool(), 0))));
    auto arity_is_even = w.app(w.app(w.app(w.app(w.arity_eliminator(), w.lit(Qualifier::u)), lam_bool), w.lit_true()), step_negate);
    EXPECT_EQ(w.lit_true(), w.app(arity_is_even, w.lit_arity(0)));
    EXPECT_EQ(w.lit_false(), w.app(arity_is_even, w.lit_arity(3)));
}

TEST(Arity, ArityRecursors) {
    World w;
    auto double_step = fe::parse(w, "??curr: *A. ??acc: *A. ASucc (???, ASucc (???, acc))");
    auto arity_double = w.app(w.app(w.app(w.arity_recursor_to_arity(), w.lit(Qualifier::u)), w.lit_arity(0)), double_step);
    EXPECT_EQ(w.lit_arity(0), w.app(arity_double, w.lit_arity(0)));
    EXPECT_EQ(w.lit_arity(8), w.app(arity_double, w.lit_arity(4)));
    auto quad_step = fe::parse(w, "??curr: *A. ??acc: *M. ??ASucc (???, curr); ASucc (???, curr)??)");
    auto arity_quad = w.app(w.app(w.app(w.arity_recursor_to_multi(), w.lit(Qualifier::u)), w.lit_arity(0)), quad_step);
    EXPECT_EQ(w.lit_arity(0), w.app(arity_quad, w.lit_arity(0)));
    EXPECT_EQ(w.variadic(w.lit_arity(3), w.lit_arity(3)), w.app(arity_quad, w.lit_arity(3)));
    auto nest_step = fe::parse(w, "??curr: *A. ??acc:*. ??ASucc (???, curr); acc??)");
    auto arity_nest = w.app(w.app(w.app(w.arity_recursor_to_star(), w.lit(Qualifier::u)), w.type_nat()), nest_step);
    EXPECT_EQ(w.type_nat(), w.app(arity_nest, w.lit_arity(0)));
    EXPECT_EQ(w.type_nat(), w.app(arity_nest, w.lit_arity(1)));
    EXPECT_EQ(w.variadic(w.lit_arity(3), w.variadic(w.lit_arity(2), w.type_nat())), w.app(arity_nest, w.lit_arity(3)));
}

TEST(Arity, DependentIndexEliminator) {
    World w;
    auto elim = w.index_eliminator();
    auto u = w.lit(Qualifier::u);
    auto elimu = w.app(elim, u);
    auto to_arity2 = fe::parse(w, "??a: *A. ??i:a. 2???");
    auto base_is_even = fe::parse(w, "??a: *A. 1???");
    auto step_is_even = fe::parse(w, "??a: *A. ??i:a. ??is_even:2???. (1???, 0???)#is_even");
    auto index_is_even = w.app(w.app(w.app(elimu, to_arity2), base_is_even), step_is_even);

    EXPECT_EQ(w.lit_index(2, 1), w.app(w.app(index_is_even, w.lit_arity(3)), w.lit_index(3, 0)));
    EXPECT_EQ(w.lit_index(2, 1), w.app(w.app(index_is_even, w.lit_arity(8)), w.lit_index(8, 4)));
    EXPECT_EQ(w.lit_index(2, 0), w.app(w.app(index_is_even, w.lit_arity(8)), w.lit_index(8, 1)));
}

TEST(Arity, MultiRecursors) {
    World w;
    EXPECT_EQ(w.lit_arity(1), w.rank(w.lit_arity(0)));
    EXPECT_EQ(w.lit_arity(3), w.rank(w.sigma({w.lit_arity(4), w.lit_arity(2), w.lit_arity(3)})));
    EXPECT_EQ(w.lit_arity(4), w.rank(w.variadic(w.lit_arity(4), w.lit_arity(2))));
}
