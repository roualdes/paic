data {
  int<lower=0> N;
  array[N] int<lower=0, upper=1> y;
  int<lower=0> K;
  matrix[N, K] X;
}
parameters {
  real alpha;
  vector[K] beta;
}
model {
  target += normal_lpdf(alpha | 0, 10) / N;
  target += normal_lpdf(beta | 0, 2.5) / N;
  y ~ bernoulli_logit_glm(X, alpha, beta);
}
