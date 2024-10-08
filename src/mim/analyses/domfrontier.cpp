#include "mim/analyses/domfrontier.h"

#include "mim/analyses/domtree.h"

namespace mim {

template<bool forward> void DomFrontierBase<forward>::create() {
    const auto& domtree = cfg().domtree();
    for (auto n : cfg().reverse_post_order().subspan(1)) {
        const auto& preds = cfg().preds(n);
        if (preds.size() > 1) {
            auto idom = domtree.idom(n);
            for (auto pred : preds)
                for (auto i = pred; i != idom; i = domtree.idom(i)) link(i, n);
        }
    }
}

template class DomFrontierBase<true>;
template class DomFrontierBase<false>;

} // namespace mim
