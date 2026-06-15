#include "model.hpp"
#include "util.hpp"
#include "welford.hpp"

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <Eigen/Dense>

#include <exception>
#include <string>

#ifdef _WIN32
#define PAIC_R_EXPORT extern "C" __declspec(dllexport)
#else
#define PAIC_R_EXPORT extern "C" __attribute__((visibility("default")))
#endif

PAIC_R_EXPORT SEXP paic_log_density(SEXP N_sexp,
                                    SEXP X_sexp,
                                    SEXP y_sexp,
                                    SEXP theta_sexp,
                                    SEXP prior_sexp) {
  try {
    paic::detail::ConstMatrixMap X = paic::detail::map_r_numeric_matrix(X_sexp, "X");
    int N = paic::detail::as_int(N_sexp, "N");
    paic::detail::ConstIntVectorMap y = paic::detail::map_r_bernoulli_vector(y_sexp, "y");
    paic::detail::ConstVectorMap theta = paic::detail::map_r_numeric_vector(theta_sexp, "theta");
    const bool prior = paic::detail::as_bool(prior_sexp, "prior");

    paic::Posterior p(N, X, y, prior);
    double ld = p.log_density(theta);

    SEXP objective = PROTECT(Rf_allocVector(REALSXP, 1));
    REAL(objective)[0] = ld;

    UNPROTECT(1);
    return objective;
  } catch (const std::exception& e) {
    Rf_error("paic_log_density failed: %s", e.what());
  } catch (...) {
    Rf_error("paic_log_density failed with an unknown C++ exception");
  }

  return R_NilValue;
}

PAIC_R_EXPORT SEXP paic_log_density_gradient(SEXP N_sexp,
                                             SEXP X_sexp,
                                             SEXP y_sexp,
                                             SEXP theta_sexp,
                                             SEXP prior_sexp) {
  try {
    paic::detail::ConstMatrixMap X = paic::detail::map_r_numeric_matrix(X_sexp, "X");
    int N = paic::detail::as_int(N_sexp, "N");
    paic::detail::ConstIntVectorMap y = paic::detail::map_r_bernoulli_vector(y_sexp, "y");
    paic::detail::ConstVectorMap theta = paic::detail::map_r_numeric_vector(theta_sexp, "theta");
    const bool prior = paic::detail::as_bool(prior_sexp, "prior");

    paic::Posterior p(N, X, y, prior);
    auto vg = p.log_density_gradient(theta);

    SEXP out = PROTECT(Rf_allocVector(VECSXP, 2));
    SEXP objective = PROTECT(Rf_allocVector(REALSXP, 1));
    REAL(objective)[0] = vg.first;
    SET_VECTOR_ELT(out, 0, objective);
    SET_VECTOR_ELT(out, 1, paic::detail::make_numeric_vector(vg.second));

    UNPROTECT(2);
    return out;
  } catch (const std::exception& e) {
    Rf_error("paic_log_density_gradient failed: %s", e.what());
  } catch (...) {
    Rf_error("paic_log_density_gradient failed with an unknown C++ exception");
  }

  return R_NilValue;
}

PAIC_R_EXPORT SEXP paic_posterior_mean_loglik(SEXP N_sexp,
                                              SEXP X_sexp,
                                              SEXP y_sexp,
                                              SEXP theta_sexp) {
  try {
    int N = paic::detail::as_int(N_sexp, "N");
    paic::detail::ConstMatrixMap X = paic::detail::map_r_numeric_matrix(X_sexp, "X");
    paic::detail::ConstIntVectorMap y = paic::detail::map_r_bernoulli_vector(y_sexp, "y");
    paic::detail::ConstMatrixMap theta = paic::detail::map_r_numeric_matrix(theta_sexp, "theta");
    Eigen::Index M = theta.rows();

    paic::detail::WelfordAccumulator w{};
    double out = 0.0;

    for (int n = 0; n < N; ++n) {
      auto Xn = X.middleRows(n, 1);
      auto yn = y.segment(n, 1);
      paic::Posterior p(N, Xn, yn, false);
      for (Eigen::Index m = 0; m < M; ++m) {
        auto thetam = theta.row(m);
        w.update(p.log_density(thetam));
      }
      out += w.mean();
      w.reset();
    }

    SEXP objective = PROTECT(Rf_allocVector(REALSXP, 1));
    REAL(objective)[0] = -2 * out;

    UNPROTECT(1);
    return objective;
  } catch (const std::exception& e) {
    Rf_error("paic_posterior_mean_loglik failed: %s", e.what());
  } catch (...) {
    Rf_error("paic_posterior_mean_loglik failed with an unknown C++ exception");
  }

  return R_NilValue;
}

PAIC_R_EXPORT SEXP paic_trace_IJ(SEXP N_sexp,
                                 SEXP X_sexp,
                                 SEXP y_sexp,
                                 SEXP theta_sexp) {
  try {
    paic::detail::ConstMatrixMap X = paic::detail::map_r_numeric_matrix(X_sexp, "X");
    int N = paic::detail::as_int(N_sexp, "N");
    paic::detail::ConstIntVectorMap y = paic::detail::map_r_bernoulli_vector(y_sexp, "y");
    paic::detail::ConstVectorMap theta = paic::detail::map_r_numeric_vector(theta_sexp, "theta");

    Eigen::Index p = theta.size();
    Eigen::MatrixXd I(p, p);
    Eigen::MatrixXd J(p, p);
    for (int n = 0; n < N; ++n) {
      auto Xn = X.middleRows(n, 1);
      auto yn = y.segment(n, 1);
      paic::Posterior p(N, Xn, yn, true);
      auto [ld, grad, hess] = p.log_density_gradient_hessian(theta);
      I += grad * grad.transpose();
      J += hess;
    }

    I /= N;
    J /= N;

    auto ldlt = J.selfadjointView<Eigen::Lower>().ldlt();
    if (ldlt.info() != Eigen::Success) {
      throw std::runtime_error("LDLT decomposition failed.");
    }

    double tr = 0.0;
    Eigen::VectorXd tmp(p);
    Eigen::VectorXd x(p);

    for (Eigen::Index pdx = 0; pdx < p; ++pdx) {
      tmp = I.col(pdx);
      x = ldlt.solve(tmp);
      tr += x(pdx);
    }

    SEXP objective = PROTECT(Rf_allocVector(REALSXP, 1));
    REAL(objective)[0] = 2 * tr;

    UNPROTECT(1);
    return objective;
  } catch (const std::exception& e) {
    Rf_error("paic_trace_IJ failed: %s", e.what());
  } catch (...) {
    Rf_error("paic_trace_IJ failed with an unknown C++ exception");
  }

  return R_NilValue;
}

static const R_CallMethodDef call_methods[] = {
  {
    "paic_log_density",
    reinterpret_cast<DL_FUNC>(&paic_log_density),
    5
  },
  {
    "paic_log_density_gradient",
    reinterpret_cast<DL_FUNC>(&paic_log_density_gradient),
    5
  },
  {
    "paic_posterior_mean_loglik",
    reinterpret_cast<DL_FUNC>(&paic_posterior_mean_loglik),
    4
  },
  {
    "paic_trace_IJ",
    reinterpret_cast<DL_FUNC>(&paic_trace_IJ),
    4
  },
  {nullptr, nullptr, 0}
};

PAIC_R_EXPORT void R_init_paic(DllInfo* dll) {
  R_registerRoutines(dll, nullptr, call_methods, nullptr, nullptr);
  R_useDynamicSymbols(dll, FALSE);
}
