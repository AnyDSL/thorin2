{-# LANGUAGE ScopedTypeVariables #-}
{-# OPTIONS_GHC -Wno-incomplete-patterns #-}
module Opt where

import Data.IORef  
bool2bit x = if x then 1 else 0
get_0_of_2 (x, _) = x
get_1_of_2 (_, x) = x
get_0_of_3 (x, _, _) = x
get_1_of_3 (_, x, _) = x
get_2_of_3 (_, _, x) = x
magic a = magic a


zero_pb_5474158_1228720 ((_5474160_1228727@(_, _5474162_1228729)):: (Int , ([Int] -> a))) = 
        let _5474162_1228725 (_1228730 :: [Int]) = 
                (_5474162_1228729 _1228730)
        in
        (_5474162_1228725 (replicate 2 (0::Int)))
eta_tup_pb_5474192_1228771 ((_5474196_1228779@(_, tup_ret_cont_5474198_1228781)):: (unit , ([Int] -> a))) = 
        let tup_ret_cont_5474198_1228777 (_1228782 :: [Int]) = 
                (tup_ret_cont_5474198_1228781 _1228782)
        in
        (tup_ret_cont_5474198_1228777 (replicate 2 (0::Int)))
eta_tup_pb_5474273_1229021 ((_5474275_1229038@(_, tup_ret_cont_5474277_1229040)):: (unit , ([Int] -> a))) = 
        let tup_ret_cont_5474277_1229031 (_1229041 :: [Int]) = 
                (tup_ret_cont_5474277_1229040 _1229041)
        in
        (tup_ret_cont_5474277_1229031 (replicate 2 (0::Int)))
f_deriv_5474001_1228345 ((_5474075_1228547@(_5474077_1228549, _5474204_1228798, _5474215_1228823)):: (Int , Int , ((Int , ((Int , ([Int] -> a)) -> a)) -> a))) = 
        let aug_f_5474023_1228437 ((_5474045_1228497@(_5474047_1228499, _5474060_1228523)):: (Int , ((Int , ((Int , ([Int] -> a)) -> a)) -> a))) = 
                let _5474060_1228521 (_1228524 :: (Int , ((Int , ([Int] -> a)) -> a))) = 
                        (_5474060_1228523 _1228524)
                in
                let aug_pow_cont_5474057_1228515 ((_5474078_1228561@(_5474080_1228563, _5474090_1228588)):: (Int , ((Int , ([Int] -> a)) -> a))) = 
                        let comp_tup_pb__5474088_1228578 ((_5474095_1228610@(_5474097_1228612, _5474111_1228641)):: (Int , ([Int] -> a))) = 
                                let _5474111_1228639 (_1228642 :: [Int]) = 
                                        (_5474111_1228641 _1228642)
                                in
                                let inner_cont_5474108_1228629 ((_5474136_1228672@[_5474137_1228673, _5474143_1228685]):: [Int]) = 
                                        let _1228660 :: Int = (_5474080_1228563 * _5474097_1228612) in
                                        let _1228678 :: Int = (_1228660 + _5474137_1228673) in
                                        (_5474111_1228639 [_1228678, _5474143_1228685])
                                in
                                let _1228617 :: Int = (_5474077_1228549 * _5474097_1228612) in
                                (_5474090_1228588 (_1228617, inner_cont_5474108_1228629))
                        in
                        let _1228568 :: Int = (_5474077_1228549 * _5474080_1228563) in
                        (_5474060_1228521 (_1228568, comp_tup_pb__5474088_1228578))
                in
                let aug_pow_else_5474024_1228457 _ = 
                        let _1228506 :: Int = ((-1::Int) + (_5474047_1228499)) in
                        (aug_f_5474023_1228437 (_1228506, aug_pow_cont_5474057_1228515))
                in
                let aug_pow_then_5474154_1228703 _ = 
                        (_5474060_1228521 ((1::Int), zero_pb_5474158_1228720))
                in
                let _1228762 :: Int = (bool2bit ((0::Int) == _5474047_1228499)) in
                (([aug_pow_else_5474024_1228457, aug_pow_then_5474154_1228703] !! _1228762) eta_tup_pb_5474192_1228771)
        in
        let _5474215_1228821 (_1228824 :: (Int , ((Int , ([Int] -> a)) -> a))) = 
                (_5474215_1228823 _1228824)
        in
        let aug_pow_cont_5474212_1228815 ((_5474218_1228848@(_5474220_1228850, _5474228_1228877)):: (Int , ((Int , ([Int] -> a)) -> a))) = 
                let comp_tup_pb__5474226_1228865 ((_5474229_1228900@(_5474231_1228902, _5474240_1228929)):: (Int , ([Int] -> a))) = 
                        let _5474240_1228927 (_1228930 :: [Int]) = 
                                (_5474240_1228929 _1228930)
                        in
                        let inner_cont_5474237_1228917 ((_5474248_1228963@[_5474249_1228964, _5474255_1228976]):: [Int]) = 
                                let _1228950 :: Int = (_5474220_1228850 * _5474231_1228902) in
                                let _1228969 :: Int = (_1228950 + _5474249_1228964) in
                                (_5474240_1228927 [_1228969, _5474255_1228976])
                        in
                        let _1228907 :: Int = (_5474077_1228549 * _5474231_1228902) in
                        (_5474228_1228877 (_1228907, inner_cont_5474237_1228917))
                in
                let _1228855 :: Int = (_5474077_1228549 * _5474220_1228850) in
                (_5474215_1228821 (_1228855, comp_tup_pb__5474226_1228865))
        in
        let aug_pow_else_5474013_1228396 _ = 
                let _1228805 :: Int = ((-1::Int) + _5474204_1228798) in
                (aug_f_5474023_1228437 (_1228805, aug_pow_cont_5474212_1228815))
        in
        let aug_pow_then_5474264_1228993 _ = 
                (_5474215_1228821 ((1::Int), zero_pb_5474158_1228720))
        in
        let _1229011 :: Int = (bool2bit ((0::Int) == _5474204_1228798)) in
        (([aug_pow_else_5474013_1228396, aug_pow_then_5474264_1228993] !! _1229011) eta_tup_pb_5474273_1229021)
-- external 
f_diffed ((__5474282_1229058@(((__5474284_1229060@[_5474285_1229061, _5474286_1229068]):: [Int]), ret_5474337_1229130)):: ([Int] , ((Int , [Int]) -> a))) = 
        let ret_5474337_1229128 (_1229131 :: (Int , [Int])) = 
                (ret_5474337_1229130 _1229131)
        in
        let ret_cont_5474294_1229081 ((__5474306_1229101@(r_5474350_1229140, pb_5474308_1229103)):: (Int , ((Int , ([Int] -> a)) -> a))) = 
                let pb_ret_cont_5474323_1229119 (__1229141 :: [Int]) = 
                        (ret_5474337_1229128 (r_5474350_1229140, __1229141))
                in
                (pb_5474308_1229103 ((1::Int), pb_ret_cont_5474323_1229119))
        in
        (f_deriv_5474001_1228345 ((__5474284_1229060 !! (0::Int)), (__5474284_1229060 !! (1::Int)), ret_cont_5474294_1229081))
-- external 
thorin_main ((__5474458_1229272@(mem_5474471_1229285, _, _, return_5474460_1229276)):: (unit , Int , (IORef (IORef Int)) , ((unit , Int) -> a))) = 
        let return_5474460_1229270 (_1229277 :: (unit , Int)) = 
                (return_5474460_1229276 _1229277)
        in
        let ret_cont_5474429_1229229 ((__5474438_1229246@(r_5474505_1229311, pb_5474440_1229248)):: (Int , ((Int , ([Int] -> a)) -> a))) = 
                let pb_ret_cont_5474448_1229261 ((__5474524_1229341@[pr_a_5474546_1229361, pr_b_5474525_1229342]):: [Int]) = 
                        let _1229318 :: Int = ((10000::Int) * r_5474505_1229311) in
                        let _1229368 :: Int = ((100::Int) * pr_a_5474546_1229361) in
                        let _1229373 :: Int = (pr_b_5474525_1229342 + _1229368) in
                        let _1229378 :: Int = (_1229318 + _1229373) in
                        (return_5474460_1229270 (mem_5474471_1229285, _1229378))
                in
                (pb_5474440_1229248 ((1::Int), pb_ret_cont_5474448_1229261))
        in
        (f_deriv_5474001_1228345 ((42::Int), (3::Int), ret_cont_5474429_1229229))