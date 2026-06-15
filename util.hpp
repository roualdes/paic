#pragma once

#include <Eigen/Dense>
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <limits>
#include <string>

namespace paic {

namespace detail {

using ConstMatrixMap =
  Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>;

using ConstVectorMap = Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, 1>>;

using ConstIntVectorMap = Eigen::Map<const Eigen::Matrix<int, Eigen::Dynamic, 1>>;

std::string as_string(SEXP x, const char* name) {
  if (!Rf_isString(x) || Rf_length(x) != 1) {
    Rf_error("%s must be a character scalar", name);
  }
  return std::string(CHAR(STRING_ELT(x, 0)));
}

bool as_bool(SEXP x, const char* name) {
  if (!Rf_isLogical(x) || Rf_length(x) != 1 || LOGICAL(x)[0] == NA_LOGICAL) {
    Rf_error("%s must be TRUE or FALSE", name);
  }
  return LOGICAL(x)[0];
}

double as_double(SEXP x, const char* name) {
  if (!Rf_isReal(x) || Rf_length(x) != 1 || !R_FINITE(REAL(x)[0])) {
    Rf_error("%s must be a finite numeric scalar", name);
  }
  return REAL(x)[0];
}

int as_int(SEXP x, const char* name) {
  if (!Rf_isInteger(x) || Rf_length(x) != 1 || INTEGER(x)[0] == NA_INTEGER) {
    Rf_error("%s must be an integer scalar", name);
  }
  return INTEGER(x)[0];
}

Eigen::VectorXd as_eigen_vector(SEXP x, const char* name) {
  if (!Rf_isReal(x)) {
    Rf_error("%s must be a numeric vector", name);
  }

  const R_xlen_t n = XLENGTH(x);
  Eigen::VectorXd out(static_cast<Eigen::Index>(n));
  for (R_xlen_t i = 0; i < n; ++i) {
    if (!R_FINITE(REAL(x)[i])) {
      Rf_error("%s contains a non-finite value", name);
    }
    out(static_cast<Eigen::Index>(i)) = REAL(x)[i];
  }
  return out;
}

Eigen::MatrixXd as_eigen_matrix(SEXP x, const char* name) {
  if (!Rf_isReal(x)) {
    Rf_error("%s must be a numeric matrix", name);
  }

  SEXP dims = Rf_getAttrib(x, R_DimSymbol);
  if (!Rf_isInteger(dims) || Rf_length(dims) != 2) {
    Rf_error("%s must be a matrix", name);
  }

  const int nrow = INTEGER(dims)[0];
  const int ncol = INTEGER(dims)[1];

  if (nrow <= 0 || ncol <= 0) {
    Rf_error("%s must have positive dimensions", name);
  }

  Eigen::MatrixXd out(static_cast<Eigen::Index>(nrow),
                      static_cast<Eigen::Index>(ncol));

  const double* ptr = REAL(x);

  // column-major
  for (int j = 0; j < ncol; ++j) {
    for (int i = 0; i < nrow; ++i) {
      const R_xlen_t idx =
        static_cast<R_xlen_t>(i) +
        static_cast<R_xlen_t>(j) * static_cast<R_xlen_t>(nrow);

      if (!R_FINITE(ptr[idx])) {
        Rf_error("%s contains a non-finite value at row %d, column %d",
                 name, i + 1, j + 1);
      }

      out(
          static_cast<Eigen::Index>(i),
          static_cast<Eigen::Index>(j)) = ptr[idx];
    }
  }

  return out;
}

SEXP make_numeric_vector(const Eigen::VectorXd& x) {
  SEXP out = PROTECT(Rf_allocVector(REALSXP, x.size()));
  for (Eigen::Index i = 0; i < x.size(); ++i) {
    REAL(out)[i] = x(i);
  }
  UNPROTECT(1);
  return out;
}

SEXP make_string(const char* x) {
  SEXP out = PROTECT(Rf_mkString(x));
  UNPROTECT(1);
  return out;
}

inline ConstMatrixMap map_r_numeric_matrix(SEXP x, const char* name) {
  if (!Rf_isReal(x)) {
    Rf_error("%s must be a double/numeric matrix", name);
  }

  SEXP dims = Rf_getAttrib(x, R_DimSymbol);
  if (!Rf_isInteger(dims) || Rf_length(dims) != 2) {
    Rf_error("%s must be a matrix", name);
  }

  const int n = INTEGER(dims)[0];
  const int p = INTEGER(dims)[1];

  if (n <= 0 || p <= 0) {
    Rf_error("%s must have positive dimensions", name);
  }

  return ConstMatrixMap(REAL(x), n, p);
}

inline ConstVectorMap map_r_numeric_vector(SEXP x, const char* name) {
  if (!Rf_isReal(x)) {
    Rf_error("%s must be a double/numeric vector", name);
  }

  const R_xlen_t n = XLENGTH(x);
  if (n <= 0) {
    Rf_error("%s must have positive length", name);
  }

  return ConstVectorMap(REAL(x), static_cast<Eigen::Index>(n));
}

inline ConstIntVectorMap map_r_int_vector(SEXP x, const char* name) {
  if (TYPEOF(x) != INTSXP) {
    Rf_error("%s must be an integer vector. In R, use as.integer(...).", name);
  }

  if (Rf_getAttrib(x, R_DimSymbol) != R_NilValue) {
    Rf_error("%s must be an integer vector, not a matrix or array", name);
  }

  const R_xlen_t n = XLENGTH(x);
  if (n <= 0) {
    Rf_error("%s must have positive length", name);
  }

  if (n > static_cast<R_xlen_t>(std::numeric_limits<Eigen::Index>::max())) {
    Rf_error("%s is too long for Eigen::Index", name);
  }

  const int* ptr = INTEGER(x);

  for (R_xlen_t i = 0; i < n; ++i) {
    if (ptr[i] == NA_INTEGER) {
      Rf_error("%s contains NA at position %lld",
               name,
               static_cast<long long>(i + 1));
    }
  }

  return ConstIntVectorMap(
                           ptr,
                           static_cast<Eigen::Index>(n));
}

inline ConstIntVectorMap map_r_bernoulli_vector(SEXP x, const char* name) {
  ConstIntVectorMap y = map_r_int_vector(x, name);

  for (Eigen::Index i = 0; i < y.size(); ++i) {
    if (!(y(i) == 0 || y(i) == 1)) {
      Rf_error("%s must contain only 0/1 values; found %d at position %lld",
               name,
               y(i),
               static_cast<long long>(i + 1));
    }
  }

  return y;
}

} // namespace detail

} // paic
