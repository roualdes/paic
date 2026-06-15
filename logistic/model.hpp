#include "base.hpp"

#include <stan/math/mix.hpp>
#include <stan/math.hpp>

#include <Eigen/Dense>

#include <tuple>
#include <utility>

namespace paic {

template <typename M, typename V>
struct Posterior : Base<Posterior<M, V>> {
 public:
  Posterior(const int& N, const M& X, const V& y, bool prior)
    : N_(N), X_(X), y_(y), prior_(prior) {}

  template <typename Derived>
  auto operator()(const Eigen::MatrixBase<Derived>& theta) const {
    using T = typename Derived::Scalar;
    using MatrixXT = typename Eigen::Matrix<T, Eigen::Dynamic, 1>;
    using stan::math::bernoulli_logit_glm_lpmf;
    using stan::math::normal_lpdf;

    const T alpha = theta(0);
    const MatrixXT beta = theta.tail(X_.cols());

    T lp = bernoulli_logit_glm_lpmf(y_, X_, alpha, beta);
    if (prior_) {
      lp += normal_lpdf(alpha, 0.0, 10.0) / N_;
      lp += normal_lpdf(beta, 0.0, 2.5) / N_;
    }

    return lp;
  }

 private:
  int N_;
  const M& X_;
  const V& y_;
  bool prior_;
};

}
