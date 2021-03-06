#include "gtest/gtest.h"

#include "thorin/world.h"
#include "thorin/fe/parser.h"

using namespace thorin;

TEST(Nominal, Sigma) {
    World w;
    auto nat = w.type_nat();
    auto nat2 = w.sigma_type(2, {"Nat x Nat"})->set(0, nat)->set(1, nat);
    EXPECT_TRUE(nat2->free_vars().none());
    EXPECT_EQ(w.pi(nat2, nat)->domain(), nat2);

    auto n42 = w.lit_nat(42);
    EXPECT_FALSE(nat2->assignable(n42));
}

TEST(Nominal, Option) {
    World w;
    auto nat = w.type_nat();

    // could replace None by unit and make it structural
    auto none = w.sigma_type(0, {"None"});
    // auto some = w.lambda(w.pi(w.kind_star(), w.kind_star()), {"Some"});
    // some->set(w.sigma({w.var(0, w.kind_star())}));
    auto o = w.lambda(w.pi(w.kind_star(), w.kind_star()), {"Option"});
    auto option_nominal = o->set(w.variant({none, o->param()}));
    // option_nominal->set(w.variant({none, w.app(some, w.var(0, w.kind_star()))}));
    auto app = w.app(option_nominal, nat);
    EXPECT_TRUE(app->isa<App>());
    EXPECT_EQ(app->op(1), nat);
    auto option = w.lambda(w.kind_star(), w.variant({w.unit(), w.var(w.kind_star(), 0)}), {"Option"});
    EXPECT_EQ(w.app(option, nat), w.variant({w.unit(), nat}));
}

TEST(Nominal, PolymorphicList) {
    World w;
    auto nat = w.type_nat();
    auto star = w.kind_star();

    // could replace Nil by unit and make it structural
    auto nil = w.sigma_type(0, {"Nil"});
    auto list = w.lambda(w.pi(star, star), {"List"});
    EXPECT_TRUE(list->is_nominal());
    auto app_var = w.app(list, list->param());
    EXPECT_TRUE(app_var->isa<App>());
    auto cons = w.sigma({list->param(), app_var});
    list->set(w.variant({nil, cons}));
    auto apped = w.app(list, nat);
    EXPECT_TRUE(apped->isa<App>());
    EXPECT_EQ(apped->op(1), nat);
}

TEST(Nominal, Nat) {
    World w;
    auto star = w.kind_star();

    auto variant = w.variant(star, 2, {"Nat"});
    variant->set(0, w.sigma_type(0, {"0"}));
    auto succ = w.sigma_type(1, {"Succ"});
    succ->set(0, variant);
    variant->set(1, succ);
    // TODO asserts, methods on nat
}

TEST(Nominal, SigmaFreeVars) {
    World w;
    auto S = w.kind_star();
    auto sig = w.sigma_type(3);
    auto v0 = w.var(S, 0);
    auto v1 = w.var(S, 1);
    sig->set(0, S)->set(1, v0)->set(2, v1);
    EXPECT_TRUE (sig->op(1)->free_vars().test(0));
    EXPECT_FALSE(sig->op(1)->free_vars().test(1));
    EXPECT_FALSE(sig->op(1)->free_vars().test(2));
    EXPECT_FALSE(sig->op(2)->free_vars().test(0));
    EXPECT_TRUE (sig->op(2)->free_vars().test(1));
    EXPECT_FALSE(sig->op(2)->free_vars().test(2));
}

TEST(Nominal, PolymorphicListVariantNominal) {
    World w;
    auto N = w.type_nat();
    auto S = w.kind_star();

    auto cons_or_nil = w.variant(S, 2, {"cons_or_nil"});
    auto list = w.lambda(w.pi(S, S), {"list"});
    auto x = list->param({"x"});
    list->set(cons_or_nil);
    auto nil = w.unit();
    auto cons = w.sigma({x, w.app(list, x)});
    cons_or_nil->set(0, nil);
    cons_or_nil->set(1, cons);
    auto apped = w.app(list, N);
    auto v = w.axiom(apped, {"v"});
    auto cons_nat = w.sigma({N, w.app(list, N)});
    auto match = w.match(v, {w.lambda(nil, w.lit_nat_16()), w.lambda(cons_nat, w.extract(w.var(cons_nat, 0), 0_s))});
    EXPECT_EQ(match->type(), N);
}

TEST(Nominal, Module) {
    World w;

    auto S = w.kind_star();
    auto B = w.type_bool();
    auto N = w.type_nat();

    // M := ??U:*. L := ??T:*. [[T, U], {nil := [] | cons := [L T]}
    auto M = w.lambda(w.pi(S, w.pi(S, S)), {"M"});
    auto U = M->param({"U"});
    auto L = w.lambda(w.pi(S, S), {"L"});
    auto T = L->param({"T"});
    auto l = w.sigma({w.sigma({T, U}),
                      w.variant({w.sigma_type(0_s, {"nil"}), w.sigma_type(1, {"cons"})->set(0, w.app(L, T))})});
    M->set(L->set(l));

    auto LNN = w.app(w.app(M, N), N);
    auto LBN = w.app(w.app(M, N), B);
    auto LNB = w.app(w.app(M, B), N);
    auto LBB = w.app(w.app(M, B), B);

    auto id = fe::parse(w, "??T:*. ??x:T. x");

    w.extract(w.axiom(LNN, {"lnn"}), 0_u64)->type()->dump();
    w.extract(w.axiom(LNB, {"lnb"}), 0_u64)->type()->dump();
    w.extract(w.axiom(LBN, {"lbn"}), 0_u64)->type()->dump();
    w.extract(w.axiom(LBB, {"lbb"}), 0_u64)->type()->dump();
    w.app(w.app(id, w.sigma({N, N})), w.extract(w.axiom(LNN, {"lnn'"}), 0_u64));
    w.app(w.app(id, w.sigma({N, B})), w.extract(w.axiom(LNB, {"lnb'"}), 0_u64));
    w.app(w.app(id, w.sigma({B, N})), w.extract(w.axiom(LBN, {"lbn'"}), 0_u64));
    w.app(w.app(id, w.sigma({B, B})), w.extract(w.axiom(LBB, {"lbb'"}), 0_u64));

    EXPECT_EQ(w.extract(w.axiom(LNN), 0_u64)->type(), w.sigma({N, N}));
    EXPECT_EQ(w.extract(w.axiom(LNB), 0_u64)->type(), w.sigma({N, B}));
    EXPECT_EQ(w.extract(w.axiom(LBN), 0_u64)->type(), w.sigma({B, N}));
    EXPECT_EQ(w.extract(w.axiom(LBB), 0_u64)->type(), w.sigma({B, B}));
}
