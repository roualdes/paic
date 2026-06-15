#include <cstddef>

namespace paic {

namespace detail {

class WelfordAccumulator {
public:
  /**
   * @brief Initialize with zero observations.
   */
  WelfordAccumulator() : n_(0), m_(0.0) {}

  /**
   * @brief Update mean, variance, and number of observations.
   *
   * @param[in] x the observation.
   */
  void update(const double& x) {
    ++n_;
    m_ += (x - m_) / static_cast<double>(n_);
  }

  /**
   * @brief Return the (scalar) mean of observations.
   *
   * @return The mean, so far.
   */
  double mean() const {
    return m_;
  }

  /**
   * @brief Return the number of observations.
   *
   * @return The number of observations, so far.
   */
  std::size_t count() const {
    return n_;
  }

  /**
   * @brief Reset the mean, variance, and number of observations.
   */
  void reset() {
    n_ = 0;
    m_ = 0.0;
  }

private:
  std::size_t n_;
  double m_;
};

} // namespace detail
} // namespace paic
