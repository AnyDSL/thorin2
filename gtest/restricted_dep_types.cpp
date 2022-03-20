#include <fstream>
#include <sstream>

#include <gtest/gtest-param-test.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "thorin/def.h"
#include "thorin/error.h"
#include "thorin/tables.h"
#include "thorin/world.h"

#include "thorin/be/ll/ll.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/rw/lower_for.h"

using namespace thorin;

TEST(RestrictedDependentTypes, join_singleton) {
    auto test_on_world = [](auto test) {
        World w;
        auto i32_t = w.type_int_width(32);
        auto i64_t = w.type_int_width(64);

        auto R = w.axiom(w.type(), w.dbg("R"));
        auto W = w.axiom(w.type(), w.dbg("W"));

        auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));
        auto DT = w.join({w.singleton(i32_t), w.singleton(i64_t)}, w.dbg("DT"));

        auto exp_pi = w.nom_pi(w.type())->set_dom({DT, RW});
        exp_pi->set_codom(w.type());
        auto Exp = w.axiom(exp_pi, w.dbg("exp"));

        test(w, R, W, Exp);
    };
    {
        std::vector<std::function<void(World&, const Def*, const Def*, const Def*, const Def*, const Def*, const Def*,
                                       const Def*, const Def*)>>
            cases;
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i32_t, R, w.op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i32_t, W, w.op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i64_t, R, w.op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i64_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam,
                      {i64_t, W, w.op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                       w.nom_lam(w.cn(i64_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float
                        w.app(exp_lam,
                              {w.type_real(32), R, w.op_bitcast(w.app(Exp, {w.vel(DT, w.type_real(32)), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(w.type_real(32)), nullptr)}),
                        TypeError);
                },
                "TypeError");
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float
                        w.app(exp_lam,
                              {w.type_real(32), W, w.op_bitcast(w.app(Exp, {w.vel(DT, w.type_real(32)), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(w.type_real(32)), nullptr)}),
                        TypeError);
                },
                "TypeError");
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // RW fail
                        w.app(exp_lam,
                              {i32_t, i32_t, w.op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, i32_t)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(i32_t), nullptr)}),
                        TypeError);
                },
                "TypeError");
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // RW fail
                        w.app(exp_lam,
                              {i64_t, i64_t, w.op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, i64_t)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(i64_t), nullptr)}),
                        TypeError);
                },
                "TypeError");
        });

        for (auto&& test : cases) {
            test_on_world([&test](World& w, auto R, auto W, auto Exp) {
                auto i32_t = w.type_int_width(32);
                auto i64_t = w.type_int_width(64);
                auto RW    = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));
                auto DT    = w.join({w.singleton(i32_t), w.singleton(i64_t)}, w.dbg("DT"));

                auto exp_sig = w.nom_sigma(4);
                exp_sig->set(0, w.type());
                exp_sig->set(1, w.type());
                exp_sig->set(2, w.app(Exp, {w.vel(DT, exp_sig->var(0_s)), w.vel(RW, exp_sig->var(1_s))}));
                exp_sig->set(3, w.cn(exp_sig->var(0_s)));

                auto exp_lam_pi = w.cn(exp_sig);
                auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
                exp_lam->app(false, exp_lam->var(3), w.op_bitcast(exp_lam->var(0_s), exp_lam->var(2_s)));
                test(w, R, W, Exp, exp_lam, DT, RW, i32_t, i64_t);
            });
        }
    }
    {
        std::vector<std::function<void(World&, const Def*, const Def*, const Def*, const Def*, const Def*, const Def*,
                                       const Def*, const Def*)>>
            cases;
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam, {i32_t, w.op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                                w.nom_lam(w.cn(i32_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NO_THROW( // no type error
                w.app(exp_lam, {i64_t, w.op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, R)}), w.lit(i64_t, 1000)),
                                w.nom_lam(w.cn(i64_t), nullptr)}));
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_NONFATAL_FAILURE( // disable until we have vel type checking..
                {
                    EXPECT_THROW( // float type error
                        w.app(exp_lam,
                              {w.type_real(32),
                               w.op_bitcast(w.app(Exp, {w.vel(DT, w.type_real(32)), w.vel(RW, R)}), w.lit(i32_t, 1000)),
                               w.nom_lam(w.cn(w.type_real(32)), nullptr)}),
                        TypeError);
                },
                "TypeError");
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_THROW( // W type error
                w.app(exp_lam, {i32_t, w.op_bitcast(w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                                w.nom_lam(w.cn(i32_t), nullptr)}),
                TypeError);
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_THROW( // W type error
                w.app(exp_lam, {i64_t, w.op_bitcast(w.app(Exp, {w.vel(DT, i64_t), w.vel(RW, W)}), w.lit(i32_t, 1000)),
                                w.nom_lam(w.cn(i64_t), nullptr)}),
                TypeError);
        });
        cases.emplace_back(
            [](World& w, auto R, auto W, auto Exp, auto exp_lam, auto DT, auto RW, auto i32_t, auto i64_t) {
            EXPECT_THROW( // float + W type error (note, the float is not yet what triggers the issue..)
                w.app(exp_lam, {w.type_real(32),
                                w.op_bitcast(w.app(Exp, {w.vel(DT, w.type_real(32)), w.vel(RW, W)}),
                                             w.lit(w.type_real(32), 1000)),
                                w.nom_lam(w.cn(w.type_real(32)), nullptr)}),
                TypeError);
        });

        for (auto&& test : cases) {
            test_on_world([&test](World& w, auto R, auto W, auto Exp) {
                auto i32_t = w.type_int_width(32);
                auto i64_t = w.type_int_width(64);
                auto RW    = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));
                auto DT    = w.join({w.singleton(i32_t), w.singleton(i64_t)}, w.dbg("DT"));

                auto exp_sig = w.nom_sigma(3);
                exp_sig->set(0, w.type());
                exp_sig->set(1, w.app(Exp, {w.vel(DT, exp_sig->var(0_s)), w.vel(RW, R)}));
                exp_sig->set(2, w.cn(exp_sig->var(0_s)));

                auto exp_lam_pi = w.cn(exp_sig);
                auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
                exp_lam->app(false, exp_lam->var(2_s), w.op_bitcast(exp_lam->var(0_s), exp_lam->var(1_s)));
                test(w, R, W, Exp, exp_lam, DT, RW, i32_t, i64_t);
            });
        }
    }
}

TEST(RestrictedDependentTypes, ll) {
    World w;
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto i64_t  = w.type_int_width(64);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));

    // Cn [mem, i32, ptr(ptr(i32, 0), 0) Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main   = w.nom_lam(main_t, w.dbg("main"));
    main->make_external();

    auto R = w.axiom(w.type(), w.dbg("R"));
    auto W = w.axiom(w.type(), w.dbg("W"));

    auto RW = w.join({w.singleton(R), w.singleton(W)}, w.dbg("RW"));

    auto DT     = w.join({w.singleton(i32_t), w.singleton(w.type_real(32))}, w.dbg("DT"));
    auto exp_pi = w.nom_pi(w.type())->set_dom({DT, RW});
    exp_pi->set_codom(w.type());

    auto Exp = w.axiom(exp_pi, w.dbg("exp"));

    auto app_exp = w.app(Exp, {w.vel(DT, i32_t), w.vel(RW, R)});

    {
        auto exp_sig = w.nom_sigma(5);
        exp_sig->set(0, mem_t);
        exp_sig->set(1, w.type());
        exp_sig->set(2, w.type());
        exp_sig->set(3, w.app(Exp, {w.vel(DT, exp_sig->var(1_s)), w.vel(RW, exp_sig->var(2_s))}));
        exp_sig->set(4, w.cn({mem_t, i32_t}));

        auto exp_lam_pi = w.cn(exp_sig);
        auto exp_lam    = w.nom_lam(exp_lam_pi, nullptr);
        auto bc         = w.op_bitcast(i32_t, exp_lam->var(3_s));
        exp_lam->app(false, exp_lam->var(4), {exp_lam->var(0_s), bc});

        main->app(false, exp_lam, {main->var(0_s), i32_t, R, w.op_bitcast(app_exp, main->var(1)), main->var(3)});
    }
    PassMan opt{w};
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<CopyProp>(br, ee);
    opt.run();

    ll::emit(w, std::cout);
}