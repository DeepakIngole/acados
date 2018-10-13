from ctypes import *
import ctypes.util 
import numpy as np
from casadi import *
from os import system

from generate_wrapper import *

#import faulthandler

#faulthandler.enable()



#class acados_integrator_model:
#	def __init__(self):
#		
#		self.type = 'explicit'
#	
#	def set(self, field, value):
#		if field=='ode_expr':
#			# call casadi codgen
#		if field=='x':
#			# set casadi variable x
#		if field=='u':
#			# set casadi variable u
#		if field=='xdot':
#			# set casadi variable xdot



#class acados_integrator_opts:
#	def __init__(self, model):
#		
#		# default
#		self.ns = 4
#		self.num_steps = 1
#		
#		if model.type=='explicit':
#			self.type = 'erk'
#	
#	def set(self, field, value):
#		if field=='ns':
#			self.ns = value
#		if field=='num_steps':
#			self.num_steps = value



class acados_integrator:
#	def __init__(self, opts, model):
	def __init__(self):
		
#		print(CasadiMeta.version())

		# load acados library
		__acados = CDLL('libacados_c.so')
		self.__acados = __acados


		# nx
		nx = 4
		nu = 1

		x = SX.sym('x', nx, 1)
		casadi_ode_expr = -2*x

		# Form a function and generate C code
		user_name = 'ode_expr'
		python_ode_expr = Function(user_name, [x], [casadi_ode_expr], ['x'], ['ode_expr'])
		cname = python_ode_expr.generate()

		oname = 'model.so'
		system('gcc -fPIC -shared '+user_name+'.c -o ' + oname)


		## load model library
		__model = CDLL('model.so')
		self.__model = __model



		## external function
		ext_fun_struct_size = __acados.external_function_casadi_struct_size()
		ext_fun_struct = cast(create_string_buffer(ext_fun_struct_size), c_void_p)
		self.ext_fun = ext_fun_struct

		## generate function pointers getter
		acados_name = 'fun'
		generate_wrapper(user_name, acados_name)

		oname = 'casadi_fun_getter.so'
		system('gcc -fPIC -shared casadi_fun_ptr_getter.c -o ' + oname + ' -L. model.so')

		__model_getter = CDLL('casadi_fun_getter.so')

		# addressof does not work !
#		tmp_ptr = cast(addressof(__model.expl_ode_fun_fun), c_void_p)
#		print(tmp_ptr)
		# pointer does not work !
#		tmp_ptr = pointer(__model.expl_ode_fun_fun)
#		print(tmp_ptr)
#		print(addressof(tmp_ptr))
		__model_getter.get_fun_fun.restype = c_void_p
#		eval("__model.get_"+name+"_fun.restype = c_void_p")
		tmp_ptr = __model_getter.get_fun_fun()
		__acados.external_function_casadi_set_fun.argtypes = [c_void_p, c_void_p]
		__acados.external_function_casadi_set_fun(self.ext_fun, tmp_ptr)

		__model_getter.get_fun_work.restype = c_void_p
		tmp_ptr = __model_getter.get_fun_work()
		__acados.external_function_casadi_set_work.argtypes = [c_void_p, c_void_p]
		__acados.external_function_casadi_set_work(self.ext_fun, tmp_ptr)

		__model_getter.get_fun_sparsity_in.restype = c_void_p
		tmp_ptr = __model_getter.get_fun_sparsity_in()
		__acados.external_function_casadi_set_sparsity_in.argtypes = [c_void_p, c_void_p]
		__acados.external_function_casadi_set_sparsity_in(self.ext_fun, tmp_ptr)

		__model_getter.get_fun_sparsity_out.restype = c_void_p
		tmp_ptr = __model_getter.get_fun_sparsity_out()
		__acados.external_function_casadi_set_sparsity_out.argtypes = [c_void_p, c_void_p]
		__acados.external_function_casadi_set_sparsity_out(self.ext_fun, tmp_ptr)

		__model_getter.get_fun_n_in.restype = c_void_p
		tmp_ptr = __model_getter.get_fun_n_in()
		__acados.external_function_casadi_set_n_in.argtypes = [c_void_p, c_void_p]
		__acados.external_function_casadi_set_n_in(self.ext_fun, tmp_ptr)

		__model_getter.get_fun_n_out.restype = c_void_p
		tmp_ptr = __model_getter.get_fun_n_out()
		__acados.external_function_casadi_set_n_out.argtypes = [c_void_p, c_void_p]
		__acados.external_function_casadi_set_n_out(self.ext_fun, tmp_ptr)

		__acados.external_function_casadi_create(self.ext_fun)



		## config
		self.config = cast(__acados.sim_config_create( 0 ), c_void_p)
		print(self.config)



		## dims
		self.dims = cast(__acados.sim_dims_create(self.config), c_void_p)
		print(self.dims)
		__acados.sim_config_set_nx(self.config, self.dims, nx)
		__acados.sim_config_set_nu(self.config, self.dims, nu)



		## opts
		self.opts = cast(__acados.sim_opts_create(self.config), c_void_p)
		print(self.opts)


	

	# TODO free stuff !!!!!!!!
	def __del__(self):
		
		self.__acados.external_function_casadi_free(self.ext_fun)
		self.__acados.sim_config_free(self.config)
		self.__acados.sim_dims_free(self.dims)
#		self.__acados.sim_opts_free(self.opts)



