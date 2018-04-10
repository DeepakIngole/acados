// standard
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
// acados
#include "acados/utils/print.h"
#include "acados/utils/mem.h"

#include "acados/sim/sim_common.h"
#include "acados/sim/sim_new_lifted_irk_integrator.h"

#include "external/blasfeo/include/blasfeo_target.h"
#include "external/blasfeo/include/blasfeo_common.h"
#include "external/blasfeo/include/blasfeo_d_aux.h"
#include "external/blasfeo/include/blasfeo_d_aux_ext_dep.h"
#include "external/blasfeo/include/blasfeo_v_aux_ext_dep.h"
#include "external/blasfeo/include/blasfeo_d_blas.h"



/************************************************
* model
************************************************/

int sim_new_lifted_irk_model_calculate_size(void *config, sim_dims *dims)
{

	int size = 0;

	size += sizeof(new_lifted_irk_model);

	return size;

}



void *sim_new_lifted_irk_model_assign(void *config, sim_dims *dims, void *raw_memory)
{

	char *c_ptr = (char *) raw_memory;

	new_lifted_irk_model *data = (new_lifted_irk_model *) c_ptr;
	c_ptr += sizeof(new_lifted_irk_model);

    assert((char*)raw_memory + sim_new_lifted_irk_model_calculate_size(config, dims) >= c_ptr);

	return data;

}



int sim_new_lifted_irk_model_set_function(void *model_, sim_function_t fun_type, void *fun)
{
    new_lifted_irk_model *model = model_;

    switch (fun_type)
    {
        case IMPL_ODE_FUN:
            model->impl_ode_fun = (external_function_generic *) fun;
            break;
        case IMPL_ODE_JAC_X:
            model->impl_ode_jac_x = (external_function_generic *) fun;
            break;
        case IMPL_ODE_JAC_XDOT:
            model->impl_ode_jac_xdot = (external_function_generic *) fun;
            break;
        case IMPL_ODE_JAC_U:
            model->impl_ode_jac_u = (external_function_generic *) fun;
            break;
        case IMPL_ODE_FUN_JAC_X_XDOT:
            model->impl_ode_fun_jac_x_xdot = (external_function_generic *) fun;
            break;
        case IMPL_ODE_JAC_X_XDOT_U:
            model->impl_ode_jac_x_xdot_u = (external_function_generic *) fun;
            break;
        case IMPL_ODE_JAC_X_U:
            model->impl_ode_jac_x_u = (external_function_generic *) fun;
            break;
        default:
            return ACADOS_FAILURE;
    }
    return ACADOS_SUCCESS;
}
/************************************************
* opts
************************************************/

int sim_new_lifted_irk_opts_calculate_size(void *config_, sim_dims *dims)
{
	int ns_max = NS_MAX;

    int size = 0;

    size += sizeof(sim_rk_opts);

    size += ns_max * ns_max * sizeof(double);  // A_mat
    size += ns_max * sizeof(double);  // b_vec
    size += ns_max * sizeof(double);  // c_vec

	int tmp0 = gauss_nodes_work_calculate_size(ns_max);
	int tmp1 = butcher_table_work_calculate_size(ns_max);
	int work_size = tmp0>tmp1 ? tmp0 : tmp1;
	size += work_size; // work

    make_int_multiple_of(8, &size);
    size += 1 * 8;

    return size;
}



void *sim_new_lifted_irk_opts_assign(void *config_, sim_dims *dims, void *raw_memory)
{
	int ns_max = NS_MAX;

    char *c_ptr = (char *) raw_memory;

    sim_rk_opts *opts = (sim_rk_opts *) c_ptr;
    c_ptr += sizeof(sim_rk_opts);

    align_char_to(8, &c_ptr);

    assign_and_advance_double(ns_max*ns_max, &opts->A_mat, &c_ptr);
    assign_and_advance_double(ns_max, &opts->b_vec, &c_ptr);
    assign_and_advance_double(ns_max, &opts->c_vec, &c_ptr);

	// work
	int tmp0 = gauss_nodes_work_calculate_size(ns_max);
	int tmp1 = butcher_table_work_calculate_size(ns_max);
	int work_size = tmp0>tmp1 ? tmp0 : tmp1;
	opts->work = c_ptr;
	c_ptr += work_size;

    assert((char*)raw_memory + sim_new_lifted_irk_opts_calculate_size(config_, dims) >= c_ptr);

    return (void *)opts;
}



void sim_new_lifted_irk_opts_initialize_default(void *config_, sim_dims *dims, void *opts_)
{
    sim_rk_opts *opts = opts_;

	opts->ns = 3; // GL 3
    int ns = opts->ns;

    assert(ns <= NS_MAX && "ns > NS_MAX!");

	// set tableau size
	opts->tableau_size = opts->ns;

	// gauss collocation nodes
    gauss_nodes(ns, opts->c_vec, opts->work);

	// butcher tableau
    butcher_table(ns, opts->c_vec, opts->b_vec, opts->A_mat, opts->work);

	// default options
    opts->newton_iter = 1;
    opts->scheme = NULL;
    opts->num_steps = 1;
    opts->num_forw_sens = dims->nx + dims->nu;
    opts->sens_forw = true;
    opts->sens_adj = false;
    opts->sens_hess = false;
    opts->jac_reuse = false;

	return;
}



void sim_new_lifted_irk_opts_update(void *config_, sim_dims *dims, void *opts_)
{
    sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    assert(ns <= NS_MAX && "ns > NS_MAX!");

	// set tableau size
	opts->tableau_size = opts->ns;

	// gauss collocation nodes
    gauss_nodes(ns, opts->c_vec, opts->work);

	// butcher tableau
    butcher_table(ns, opts->c_vec, opts->b_vec, opts->A_mat, opts->work);

	return;
}



/************************************************
* memory
************************************************/

int sim_new_lifted_irk_memory_calculate_size(void *config, sim_dims *dims, void *opts_)
{
	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    int nx = dims->nx;
    int nu = dims->nu;

    // int steps = opts->num_steps;

    int size = sizeof(sim_new_lifted_irk_memory);
    
    size += 3*sizeof(struct blasfeo_dmat); // JGK, JGf, JKf
    size += 1*sizeof(struct blasfeo_dvec); // K
    size += 2*sizeof(struct blasfeo_dvec); // x, u 
    
    size += blasfeo_memsize_dmat(nx*ns, nx*ns); // JGK
    size += 2*blasfeo_memsize_dmat(nx*ns, nx+nu); // JGf, JKf
    size += 1*blasfeo_memsize_dvec(nx*ns); // K
    size += 1*blasfeo_memsize_dvec(nx);    // x
    size += 1*blasfeo_memsize_dvec(nu);    // u

    make_int_multiple_of(64, &size);
    size += 1 * 64;

    return size;
}



void *sim_new_lifted_irk_memory_assign(void *config, sim_dims *dims, void *opts_, void *raw_memory)
{
    char *c_ptr = (char *)raw_memory;

	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    int nx = dims->nx;
    int nu = dims->nu;

    // int steps = opts->num_steps;

    sim_new_lifted_irk_memory *memory = (sim_new_lifted_irk_memory *) c_ptr;
    c_ptr += sizeof(sim_new_lifted_irk_memory);

    memory->JGK = (struct blasfeo_dmat *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dmat);

    memory->JGf = (struct blasfeo_dmat *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dmat);

    memory->JKf = (struct blasfeo_dmat *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dmat);

    memory->K = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    memory->x = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);
    
    memory->u = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    align_char_to(64, &c_ptr);

    assign_and_advance_blasfeo_dmat_mem(nx*ns, nx*ns, memory->JGK, &c_ptr);
    assign_and_advance_blasfeo_dmat_mem(nx*ns, nx+nu, memory->JGf, &c_ptr);
    assign_and_advance_blasfeo_dmat_mem(nx*ns, nx+nu, memory->JKf, &c_ptr);
    blasfeo_dgese(nx*ns, nx+nu, 0.0, memory->JKf, 0, 0);

    assign_and_advance_blasfeo_dvec_mem(nx*ns, memory->K, &c_ptr);
    blasfeo_dvecse(nx*ns, 0.0, memory->K, 0);
    assign_and_advance_blasfeo_dvec_mem(nx, memory->x, &c_ptr);
    blasfeo_dvecse(nx, 0.0, memory->x, 0);
    assign_and_advance_blasfeo_dvec_mem(nu, memory->u, &c_ptr);
    blasfeo_dvecse(nu, 0.0, memory->u, 0);
    
    // TODO(andrea): need to move this to options.
    memory->update_sens = 1;

    assert((char*)raw_memory + sim_new_lifted_irk_memory_calculate_size(config, dims, opts_) >= c_ptr);

    return (void *)memory;
}



/************************************************
* workspace
************************************************/

int sim_new_lifted_irk_workspace_calculate_size(void *config_, sim_dims *dims, void *opts_)
{
	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    int nx = dims->nx;
    int nu = dims->nu;

    int steps = opts->num_steps;
    
    int size = sizeof(sim_new_lifted_irk_workspace);

    size += 1*sizeof(struct blasfeo_dmat); // S_forw
    size += steps*sizeof(struct blasfeo_dmat); // JG_traj

    size += 3*sizeof(struct blasfeo_dvec); // rG, xt, xn
    size += 2*sizeof(struct blasfeo_dvec); // lambda, lambdaK
    size += 2*steps*sizeof(struct blasfeo_dvec);  // **xn_traj, **K_traj;
    size += 1*sizeof(struct blasfeo_dvec);  // w ([x; u]) 

    size += blasfeo_memsize_dmat(nx, nx+nu); // S_forw
    size += steps * blasfeo_memsize_dmat(nx*ns, nx*ns); // for JG_traj

    size += 1*blasfeo_memsize_dvec(nx*ns); // K
    size += 2*blasfeo_memsize_dvec(nx); // xt, x
    size += blasfeo_memsize_dvec(nx+nu); // lambda
    size += blasfeo_memsize_dvec(nx*ns); // lambdaK
    size += blasfeo_memsize_dvec(nx + nu); // w
    size += steps * blasfeo_memsize_dvec(nx); // for xn_traj
    size += steps * blasfeo_memsize_dvec(nx*ns); // for K_traj

    size += nx * sizeof(double); //  rGt
    size += nx * (2*nx+nu) * sizeof(double); // jac_out
    size += nx * nx * sizeof(double); // Jt
    size += (2*nx + nu) * sizeof(double); // ode_args
    size += (nx+nu) * sizeof(double); // S_adj_w

    size += nx *ns * sizeof(int); // ipiv

    make_int_multiple_of(64, &size);
    size += 1 * 64;

    return size;
}



static void *sim_new_lifted_irk_workspace_cast(void *config_, sim_dims *dims, void *opts_, void *raw_memory)
{
	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    int nx = dims->nx;
    int nu = dims->nu;

    int steps = opts->num_steps;

    char *c_ptr = (char *)raw_memory;

    sim_new_lifted_irk_workspace *workspace = (sim_new_lifted_irk_workspace *) c_ptr;
    c_ptr += sizeof(sim_new_lifted_irk_workspace);

    assign_and_advance_blasfeo_dmat_structs(steps, &workspace->JG_traj, &c_ptr);

    workspace->S_forw = (struct blasfeo_dmat *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dmat);


    assign_and_advance_blasfeo_dvec_structs(steps, &workspace->xn_traj, &c_ptr);
    assign_and_advance_blasfeo_dvec_structs(steps, &workspace->K_traj, &c_ptr);

    workspace->rG = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    workspace->xt = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    workspace->xn = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    workspace->w = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    workspace->lambda = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    workspace->lambdaK = (struct blasfeo_dvec *)c_ptr;
    c_ptr += sizeof(struct blasfeo_dvec);

    align_char_to(64, &c_ptr);

    assign_and_advance_blasfeo_dmat_mem(nx, nx+nu, workspace->S_forw, &c_ptr);
    for (int i=0;i<steps;i++){
        assign_and_advance_blasfeo_dmat_mem(nx*ns, nx*ns, &workspace->JG_traj[i], &c_ptr);
    }
    
    assign_and_advance_blasfeo_dvec_mem(nx*ns, workspace->rG, &c_ptr);
    assign_and_advance_blasfeo_dvec_mem(nx, workspace->xt, &c_ptr);
    assign_and_advance_blasfeo_dvec_mem(nx, workspace->xn, &c_ptr);
    assign_and_advance_blasfeo_dvec_mem(nx+nu, workspace->w, &c_ptr);
    assign_and_advance_blasfeo_dvec_mem(nx+nu, workspace->lambda, &c_ptr);
    assign_and_advance_blasfeo_dvec_mem(nx*ns, workspace->lambdaK, &c_ptr);

    for (int i=0;i<steps;i++){
        assign_and_advance_blasfeo_dvec_mem(nx, &workspace->xn_traj[i], &c_ptr);
        assign_and_advance_blasfeo_dvec_mem(nx*ns, &workspace->K_traj[i], &c_ptr);
    }

    assign_and_advance_double(nx, &workspace->rGt, &c_ptr);

    assign_and_advance_double(nx * (2*nx+nu), &workspace->jac_out, &c_ptr);
    assign_and_advance_double(nx * nx, &workspace->Jt, &c_ptr);
    assign_and_advance_double(2*nx + nu, &workspace->ode_args, &c_ptr);
    assign_and_advance_double(nx + nu, &workspace->S_adj_w, &c_ptr);

    assign_and_advance_int(nx * ns , &workspace->ipiv, &c_ptr);

    assert((char*)raw_memory + sim_new_lifted_irk_workspace_calculate_size(config_, dims, opts_) >= c_ptr);

    return (void *)workspace;
}



/************************************************
* functions
************************************************/

int sim_new_lifted_irk(void *config_, sim_in *in, sim_out *out, void *opts_, void *mem_, void *work_)
{

	sim_solver_config *config = config_;
	sim_rk_opts *opts = opts_;

    assert(opts->ns == opts->tableau_size && "the Butcher tableau size does not match ns");

    int ns = opts->ns;

    sim_dims *dims = in->dims;
    sim_new_lifted_irk_workspace *workspace = (sim_new_lifted_irk_workspace *) 
        sim_new_lifted_irk_workspace_cast(config, dims, opts, work_);
    
    sim_new_lifted_irk_memory *memory = (sim_new_lifted_irk_memory *) mem_;

    int ii, jj, kk, ss;
    double a;

    int nx = dims->nx;
	int nu = dims->nu;
    double *x = in->x;
	double *u = in->u;
    double *S_forw_in = in->S_forw;

    int newton_iter = opts->newton_iter;
    double *A_mat = opts->A_mat;
    double *b_vec = opts->b_vec;
    int num_steps = opts->num_steps;
    if (num_steps > 1)
    {
        printf("FORWARD-BACKWARD SWEEP NECESSARY TO USE num_steps > 1 \
                NOT IMPLEMENTED YET - EXITING.");
        exit(1);
    }

    double step = in->T/num_steps;
    int update_sens = memory->update_sens;

	double *rGt = workspace->rGt;
	double *jac_out = workspace->jac_out;
    double *Jt = workspace->Jt;
    double *ode_args = workspace->ode_args;
    int *ipiv = workspace->ipiv;
	struct blasfeo_dmat *JGK = memory->JGK;
    struct blasfeo_dmat *S_forw = workspace->S_forw;
	struct blasfeo_dvec *rG = workspace->rG;
    struct blasfeo_dvec *K = memory->K;
    struct blasfeo_dmat *JGf = memory->JGf;
    struct blasfeo_dmat *JKf = memory->JKf;
    struct blasfeo_dvec *xt = workspace->xt;
    struct blasfeo_dvec *xn = workspace->xn;

    // for adjoint
    struct blasfeo_dmat *JG_traj = workspace->JG_traj;
    struct blasfeo_dvec *lambda = workspace->lambda;
    struct blasfeo_dvec *lambdaK = workspace->lambdaK;
    struct blasfeo_dvec *xn_traj = workspace->xn_traj;
    struct blasfeo_dvec *K_traj = workspace->K_traj;
    struct blasfeo_dvec *w = workspace->w;
    double *S_adj_in = workspace->S_adj_w;

    double *x_out = out->xn;
    double *S_forw_out = out->S_forw;
    double *S_adj_out = out->S_adj;

	ext_fun_arg_t ext_fun_type_in[5];
	void *ext_fun_in[5]; // XXX large enough ?
	ext_fun_arg_t ext_fun_type_out[5];
	void *ext_fun_out[5]; // XXX large enough ?

	new_lifted_irk_model *model = in->model;

    acados_timer timer, timer_ad;
    double timing_ad = 0.0;

    if (opts->sens_adj)
    {
        printf("NOT IMPLEMENTED YET - EXITING.");
        exit(1);
    }
    
    // initialize
    blasfeo_dgese(nx*ns, nx*ns, 0.0, JGK, 0, 0);
    blasfeo_dgese(nx*ns, nx+nu, 0.0, JGf, 0, 0);
    
    // TODO(dimitris): shouldn't this be NF instead of nx+nu??
    blasfeo_pack_dmat(nx, nx+nu, S_forw_in, nx, S_forw, 0, 0);
    // blasfeo_print_dmat(nx, nx+nu, S_forw, 0, 0);

    blasfeo_dvecse(nx*ns, 0.0, rG, 0);
    blasfeo_pack_dvec(nx, x, xn, 0);

    blasfeo_dvecse(nx*ns, 0.0, lambdaK, 0);

    for(kk=0;kk<nx;kk++)
        S_adj_in[kk] = in->S_adj[kk];
    for(kk=0;kk<nu;kk++)
        S_adj_in[nx+kk] = 0.0;
    blasfeo_pack_dvec(nx+nu, S_adj_in, lambda, 0);

    for (kk=0;kk<2*nx;kk++) //initialize x,xdot with zeros
        ode_args[kk] = 0.0;
    for (kk=0;kk<nu;kk++) //set controls
        ode_args[2*nx+kk] = u[kk];

    for (kk=0;kk<nx;kk++)
        rGt[kk] = 0.0;
    for (kk=0;kk<nx*(2*nx+nu);kk++)
        jac_out[kk] = 0.0;
    for (kk=0;kk<nx*nx;kk++)
        Jt[kk] = 0.0;
	
    // start the loop
    acados_tic(&timer);
    for(ss=0; ss<num_steps; ss++)
	{
        // expansion step (only K variable)
        // compute x and u step
        blasfeo_pack_dvec(nx, in->x, w, 0);
        blasfeo_pack_dvec(nu, in->u, w, nx);

        blasfeo_daxpy(nx, -1.0, memory->x, 0, w, 0, w, 0);
        blasfeo_daxpy(nu, -1.0, memory->u, 0, w, nx, w, nx);
        blasfeo_dgemv_n(nx*ns, nx+nu, 1.0, JKf, 0, 0, w, 0, 1.0, K, 0, K, 0);
        
        blasfeo_pack_dvec(nx, in->x, memory->x, 0);
        blasfeo_pack_dvec(nu, in->u, memory->u, 0);
        
        // reset value of JKf 
        blasfeo_dgese(nx*ns, nx+nu, 0.0, JKf, 0, 0);
        
        int iter;
        for(iter = 0; iter < newton_iter; iter++) {
            for(ii=0; ii<ns; ii++) // ii-th row of tableau
            {
                // take x(n); copy a strvec into a strvec
                blasfeo_dveccp(nx, xn, 0, xt, 0);

                for(jj=0; jj<ns; jj++)
                { // jj-th col of tableau
                    a = A_mat[ii+ns*jj];
                    if(a!=0)
                    {           // xt = xt + T_int * a[i,j]*K_j
                        a *= step;
                        blasfeo_daxpy(nx, a, K, jj*nx, xt, 0, xt, 0);
                    }
                }
                // put xn+sum kj into first nx elements of ode_arg
                blasfeo_unpack_dvec(nx, xt, 0, ode_args);

                // put ki into the next nx elements of ode_args
                blasfeo_unpack_dvec(nx, K, ii*nx, ode_args+nx);

                // compute the residual of implicit ode at time t_ii, store value in rGt
                acados_tic(&timer_ad);
                ext_fun_type_in[0] = COLMAJ;
                ext_fun_in[0] = ode_args+0; // x: nx
                ext_fun_type_in[1] = COLMAJ;
                ext_fun_in[1] = ode_args+nx; // dx: nx
                ext_fun_type_in[2] = COLMAJ;
                ext_fun_in[2] = ode_args+nx+nx; // u: nu

                ext_fun_type_out[0] = COLMAJ;
                ext_fun_out[0] = rGt+0; // fun: nx

                model->impl_ode_fun->evaluate(model->impl_ode_fun, ext_fun_type_in, ext_fun_in, ext_fun_type_out, ext_fun_out);
                timing_ad += acados_toc(&timer_ad);

                // fill in elements of rG  - store values rGt on (ii*nx)th position of rG
                blasfeo_pack_dvec(nx, rGt, rG, ii*nx);

                if ( update_sens )
                {
                    // compute the jacobian of implicit ode
                    acados_tic(&timer_ad);
					ext_fun_type_in[0] = COLMAJ;
					ext_fun_in[0] = ode_args+0; // x: nx
					ext_fun_type_in[1] = COLMAJ;
					ext_fun_in[1] = ode_args+nx; // dx: nx
					ext_fun_type_in[2] = COLMAJ;
					ext_fun_in[2] = ode_args+nx+nx; // u: nu

					ext_fun_type_out[0] = COLMAJ;
					ext_fun_out[0] = jac_out+0; // fun: nx
					ext_fun_type_out[1] = COLMAJ;
					ext_fun_out[1] = jac_out+nx; // jac_x: nx*nx
					ext_fun_type_out[2] = COLMAJ;
					ext_fun_out[2] = jac_out+nx+nx*nx; // jac_xdot: nx*nx

                    model->impl_ode_fun_jac_x_xdot->evaluate(
                            model->impl_ode_fun_jac_x_xdot, 
                            ext_fun_type_in, ext_fun_in, ext_fun_type_out, 
                            ext_fun_out);
                    
                    timing_ad += acados_toc(&timer_ad);
                    blasfeo_pack_dvec(nx, jac_out, rG, ii*nx);

                    // compute the blocks of JGK
                    for (jj=0; jj<ns; jj++)
                    { //compute the block (ii,jj)th block = Jt
                        a = A_mat[ii + ns*jj];
                        if (a!=0)
                        {
                            a *= step;
                            for (kk=0; kk<nx*nx; kk++)
                                Jt[kk] = a * jac_out[kk+nx];
                        }
                        if(jj==ii)
                        {
                            for (kk=0; kk<nx*nx; kk++)
                                Jt[kk] += jac_out[nx*(nx+1)+kk];
                                // Jt[kk] += jac_out[nx*nx+kk];
                        }
                        // fill in the ii-th, jj-th block of JGK
                        blasfeo_pack_dmat(nx, nx, Jt, nx, JGK, ii*nx, jj*nx);
                    } // end jj


                    blasfeo_dveccp(nx, xn, 0, xt, 0);
                    for(jj=0; jj<ns; jj++)
                    {
                        a = A_mat[ii+ns*jj];
                        if(a!=0)
                        {
                            a *= step;
                            blasfeo_daxpy(nx, a, K, jj*nx, xt, 0, xt, 0);
                        }
                    }

                    blasfeo_unpack_dvec(nx, xt, 0, ode_args);
                    blasfeo_unpack_dvec(nx, K, ii*nx, ode_args+nx);
                    
                    acados_tic(&timer_ad);
                    
                    ext_fun_type_in[0] = COLMAJ;
                    ext_fun_in[0] = ode_args+0; // x: nx
                    ext_fun_type_in[1] = COLMAJ;
                    ext_fun_in[1] = ode_args+nx; // dx: nx
                    ext_fun_type_in[2] = COLMAJ;
                    ext_fun_in[2] = ode_args+nx+nx; // u: nu

                    ext_fun_type_out[0] = COLMAJ;
                    ext_fun_out[0] = jac_out+0; // jac_x: nx*nx
                    ext_fun_type_out[1] = COLMAJ;
                    ext_fun_out[1] = jac_out+nx*nx; // jac_xdot: nx*nx
                    ext_fun_type_out[2] = COLMAJ;
                    ext_fun_out[2] = jac_out+nx*nx+nx*nx; // jac_u: nx*nu

                    model->impl_ode_jac_x_xdot_u->evaluate(
                            model->impl_ode_jac_x_xdot_u, 
                            ext_fun_type_in, ext_fun_in, 
                            ext_fun_type_out, ext_fun_out);

                    timing_ad += acados_toc(&timer_ad);
                    // extract Jacobian of G wrt x
                    blasfeo_pack_dmat(nx, nx, jac_out, nx, JGf, ii*nx, 0);              
                    // extract Jacobian of G wrt u
                    blasfeo_pack_dmat(nx, nu, jac_out+2*nx*nx, nx, JGf, ii*nx, nx);     
                }
            } // end ii
        }
        //DGETRF computes an LU factorization of a general M-by-N matrix A
        //using partial pivoting with row interchanges.

        if ( update_sens )
        {
            blasfeo_dgetrf_rowpivot(nx*ns, nx*ns, JGK, 0, 0, JGK, 0, 0, ipiv);
        }

        // permute also the r.h.s
        blasfeo_dvecpe(nx*ns, ipiv, rG, 0);

        // solve JGK * y = rG, JGK on the (l)eft, (l)ower-trian, (n)o-trans
        //                    (u)nit trian
        blasfeo_dtrsv_lnu(nx*ns, JGK, 0, 0, rG, 0, rG, 0);

        // solve JGK * x = rG, JGK on the (l)eft, (u)pper-trian, (n)o-trans
        //                    (n)o unit trian , and store x in rG
        blasfeo_dtrsv_unn(nx*ns, JGK, 0, 0, rG, 0, rG, 0);
        // scale and add a generic strmat into a generic strmat // K = K - rG, where rG is DeltaK
        blasfeo_daxpy(nx*ns, -1.0, rG, 0, K, 0, K, 0);

        // evaluate forward sensitivities

        // obtain JKf
        blasfeo_dgemm_nn(nx*ns, nx+nu, nx, 1.0, JGf, 0, 0, S_forw, 0, 0, 0.0, JKf, 0, 0, JKf, 0, 0);
        
        blasfeo_dgead(nx*ns, nu, 1.0, JGf, 0, nx, JKf, 0, nx);

        blasfeo_drowpe(nx*ns, ipiv, JKf);
        blasfeo_dtrsm_llnu(nx*ns, nx+nu, 1.0, JGK, 0, 0, JKf, 0, 0, JKf, 0, 0);
        blasfeo_dtrsm_lunn(nx*ns, nx+nu, 1.0, JGK, 0, 0, JKf, 0, 0, JKf, 0, 0);

        // update forward sensitivity
        for(jj=0; jj<ns; jj++)
            blasfeo_dgead(nx, nx+nu, -step*b_vec[jj], JKf, jj*nx, 0, S_forw, 0, 0);

        // obtain x(n+1)
        for(ii=0;ii<ns;ii++)
            blasfeo_daxpy(nx, step*b_vec[ii], K, ii*nx, xn, 0, xn, 0);

    } // end int step ss

    // extract output

    blasfeo_unpack_dvec(nx, xn, 0, x_out);

    blasfeo_unpack_dmat(nx, nx+nu, S_forw, 0, 0, S_forw_out, nx);

    out->info->CPUtime = acados_toc(&timer);
    out->info->LAtime = 0.0;
    out->info->ADtime = timing_ad;

    return 0;
}



void sim_new_lifted_irk_config_initialize_default(void *config_)
{

	sim_solver_config *config = config_;

	config->evaluate = &sim_new_lifted_irk;
	config->opts_calculate_size = &sim_new_lifted_irk_opts_calculate_size;
	config->opts_assign = &sim_new_lifted_irk_opts_assign;
	config->opts_initialize_default = &sim_new_lifted_irk_opts_initialize_default;
	config->opts_update = &sim_new_lifted_irk_opts_update;
	config->memory_calculate_size = &sim_new_lifted_irk_memory_calculate_size;
	config->memory_assign = &sim_new_lifted_irk_memory_assign;
	config->workspace_calculate_size = &sim_new_lifted_irk_workspace_calculate_size;
	config->model_calculate_size = &sim_new_lifted_irk_model_calculate_size;
	config->model_assign = &sim_new_lifted_irk_model_assign;
    config->model_set_function = &sim_new_lifted_irk_model_set_function;

	return;

}