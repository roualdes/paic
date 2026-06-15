#include <stan/math/mix.hpp>
#include <stan/math.hpp>

#include <Eigen/Dense>

#include <cmath>
#include <tuple>
#include <utility>

namespace paic {

template <class C>
class Base {
public:
  template <typename Derived>
  double log_density(const Eigen::MatrixBase<Derived>& theta) const {
    return self()(theta);
  }

  template <typename Derived>
  std::pair<double, Eigen::VectorXd>
  log_density_gradient(const Eigen::MatrixBase<Derived>& theta) const {
    double value = 0.0;
    Eigen::VectorXd gradient;
    stan::math::gradient(self(), theta, value, gradient);
    return std::make_pair(value, gradient);
  }

  template <typename Derived>
  std::tuple<double, Eigen::VectorXd, Eigen::MatrixXd>
  log_density_gradient_hessian(const Eigen::MatrixBase<Derived>& theta) const {
    double value = 0.0;
    Eigen::VectorXd gradient;
    Eigen::MatrixXd hessian;
    stan::math::hessian(self(), theta, value, gradient, hessian);
    return std::make_tuple(value, gradient, hessian);
  }

private:
  const C& self() const {
    return static_cast<const C&>(*this);
  }
};

} // namespace paic
