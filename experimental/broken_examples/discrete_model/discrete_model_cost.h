/* This function was automatically generated by CasADi */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef real_t
#define real_t double
#endif /* real_t */

int discrete_model_cost(const real_t** arg, real_t** res, int* iw, real_t* w, int mem);
void discrete_model_cost_incref(void);
void discrete_model_cost_decref(void);
int discrete_model_cost_n_in(void);
int discrete_model_cost_n_out(void);
const char* discrete_model_cost_name_in(int i);
const char* discrete_model_cost_name_out(int i);
const int* discrete_model_cost_sparsity_in(int i);
const int* discrete_model_cost_sparsity_out(int i);
int discrete_model_cost_work(int *sz_arg, int* sz_res, int *sz_iw, int *sz_w);
#ifdef __cplusplus
} /* extern "C" */
#endif
