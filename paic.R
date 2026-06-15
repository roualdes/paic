library(jsonlite)
library(cmdstanr)

data <- fromJSON("logistic/data.json")
dyn.load("build/lib/logistic_r.so") # change extension?

csm <- cmdstan_model("logistic/logistic.stan")
post <- csm$sample("logistic/data.json")
draws <- post$draws(format = "matrix")[,-1]
map <- csm$optimize(data, jacobian = TRUE)$draws()[-1]

.Call("paic_log_density", data$N, data$X, data$y, map, TRUE)

fit <- .Call("paic_log_density_gradient", data$N, data$X, data$y, map, TRUE)
names(fit) <- c("log_density", "gradient")
fit

one <- .Call("paic_trace_IJ", data$N, data$X, data$y, map)
two <- .Call("paic_posterior_mean_loglik", data$N, data$X, data$y, draws)
(paic <- one + two)
