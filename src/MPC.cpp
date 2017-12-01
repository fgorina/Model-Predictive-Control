#include "MPC.h"
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include "Eigen-3.3/Eigen/Core"
#include <math.h>

using CppAD::AD;

size_t N = 15;
double dt = 0.05;

// This value assumes the model presented in the classroom is used.
//
// It was obtained by measuring the radius formed by running the vehicle in the
// simulator around in a circle with a constant steering angle and velocity on a
// flat terrain.
//
// Lf was tuned until the the radius formed by the simulating the model
// presented in the classroom matched the previous radius.
//

// Some values : N 25, dt 0.05 // Coefs 50/150, till 0, 2500
// This is the length from front to CoG that has a similar radius.
const double Lf = 2.67;
double ref_v = 60;

// Parameters for speed regulator.
//  When te road is straight we try to get maximum speed
//  When it mades turns we reduce speed according the factor.
//  A factor of 2 means reduce to halve speed when road turns 45º
// Minimum speed is the minimum speed to drive
double max_v = 100.0;   // target màximum value
double min_v = 45.0;   // target màximum value
double dec_factor = 2.0; // Factor to deccelerate when bendy

// The solver takes all the state variables and actuator
// variables in a singular vector. Thus, we should to establish
// when one variable starts and another ends to make our lifes easier.
size_t x_start = 0;
size_t y_start = x_start + N;
size_t psi_start = y_start + N;
size_t v_start = psi_start + N;
size_t cte_start = v_start + N;
size_t epsi_start = cte_start + N;
size_t delta_start = epsi_start + N;
size_t a_start = delta_start + N - 1;


class FG_eval {
public:
    Eigen::VectorXd coeffs;
    // Coefficients of the fitted polynomial.
    FG_eval(Eigen::VectorXd coeffs) { this->coeffs = coeffs; }
    
    typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
    // `fg` is a vector containing the cost and constraints.
    // `vars` is a vector containing the variable values (state & actuators).
    
    
    
    // Evaluate a polynomial.
    double polyeval1(Eigen::VectorXd coeffs,  double x) {
        double result = 0.0;
        for (int i = 0; i < coeffs.size(); i++) {
            result += coeffs[i] * pow(x, i);
        }
        return result;
    }
    
    void operator()(ADvector& fg, const ADvector& vars) {
        
        // The cost is stored is the first element of `fg`.
        // Any additions to the cost should be added to `fg[0]`.
        fg[0] = 0;
        
        // The part of the cost based on the reference state.
        for (int t = 0; t < N; t++) {
            fg[0] += CppAD::pow(vars[cte_start + t], 2);
            fg[0] += CppAD::pow(vars[epsi_start + t], 2);
            fg[0] += CppAD::pow(vars[v_start + t] - ref_v, 2);
        }
        
        // Minimize the use of actuators.
        
        for (int t = 0; t < N - 1; t++) {
            fg[0] += 150*CppAD::pow(vars[delta_start + t], 2);
            fg[0] += CppAD::pow(vars[a_start + t], 2);
        }
        
        // Minimize the value gap between sequential actuations.
        for (int t = 0; t < N - 2; t++) {
            fg[0] += 2000.0*CppAD::pow(vars[delta_start + t + 1] - vars[delta_start + t], 2);
            fg[0] += CppAD::pow(vars[a_start + t + 1] - vars[a_start + t], 2);
        }
        
        
        //
        // Setup Constraints
        //
        // NOTE: In this section you'll setup the model constraints.
        
        // Initial constraints
        //
        // We add 1 to each of the starting indices due to cost being located at
        // index 0 of `fg`.
        // This bumps up the position of all the other values.
        fg[1 + x_start] = vars[x_start];
        fg[1 + y_start] = vars[y_start];
        fg[1 + psi_start] = vars[psi_start];
        fg[1 + v_start] = vars[v_start];
        fg[1 + cte_start] = vars[cte_start];
        fg[1 + epsi_start] = vars[epsi_start];
        
        // The rest of the constraints
        for (int t = 1; t < N; t++) {
            // The state at time t+1 .
            AD<double> x1 = vars[x_start + t];
            AD<double> y1 = vars[y_start + t];
            AD<double> psi1 = vars[psi_start + t];
            AD<double> v1 = vars[v_start + t];
            AD<double> cte1 = vars[cte_start + t];
            AD<double> epsi1 = vars[epsi_start + t];
            
            // The state at time t.
            AD<double> x0 = vars[x_start + t - 1];
            AD<double> y0 = vars[y_start + t - 1];
            AD<double> psi0 = vars[psi_start + t - 1];
            AD<double> v0 = vars[v_start + t - 1];
            AD<double> cte0 = vars[cte_start + t - 1];
            AD<double> epsi0 = vars[epsi_start + t - 1];
            
            // Only consider the actuation at time t.
            AD<double> delta0 = vars[delta_start + t - 1];
            AD<double> a0 = vars[a_start + t - 1];
            
            AD<double> f0 = coeffs[0] + coeffs[1] * x0 + coeffs[2] * x0*x0 + coeffs[3]*x0*x0*x0;
            AD<double> x01 = x0 + v0 * CppAD::cos(psi0) * dt;
            AD<double> f1 = coeffs[0] + coeffs[1] * x01 + coeffs[2] * x01*x01 + coeffs[3] * x01*x01*x01 ;
            
            //AD<double> psides0 = CppAD::atan(coeffs[1]);
            AD<double> psides0 = CppAD::atan(coeffs[1] + coeffs[2]*x0 + coeffs[3]*x0*x0);
            
            // Here's `x` to get you started.
            // The idea here is to constraint this value to be 0.
            //
            // Recall the equations for the model:
            // x_[t+1] = x[t] + v[t] * cos(psi[t]) * dt
            // y_[t+1] = y[t] + v[t] * sin(psi[t]) * dt
            // psi_[t+1] = psi[t] + v[t] / Lf * delta[t] * dt
            // v_[t+1] = v[t] + a[t] * dt
            // cte[t+1] = f(x[t]) - y[t] + v[t] * sin(epsi[t]) * dt -
            // epsi[t+1] = psi[t] - psides[t] + v[t] * delta[t] / Lf * dt
            
            fg[1 + x_start + t] = x1 - (x0 + v0 * CppAD::cos(psi0) * dt);
            fg[1 + y_start + t] = y1 - (y0 + v0 * CppAD::sin(psi0) * dt);
            fg[1 + psi_start + t] = psi1 - (psi0 + v0 * delta0 / Lf * dt);
            fg[1 + v_start + t] = v1 - (v0 + a0 * dt);
            fg[1 + cte_start + t] = cte1 - (f1-(y0 + v0 * CppAD::sin(psi0) * dt));
            //fg[1 + cte_start + t] = cte1 - ((f0 - y0) + ((v0 * CppAD::sin(epsi0) * dt) ));
            fg[1 + epsi_start + t] = epsi1 - ((psi0 - psides0) + v0 * delta0 / Lf * dt);
        }
    }
};

//
// MPC class definition implementation.
//
MPC::MPC() {}
MPC::~MPC() {}


vector<double> MPC::Solve(Eigen::VectorXd state, Eigen::VectorXd coeffs) {
    //size_t i;
    //typedef CPPAD_TESTVECTOR(double) Dvector;
    
    
    
    double x = state[0];
    double y = state[1];
    double psi = state[2];
    double v = state[3];
    double cte = state[4];
    double epsi = state[5];
    
    // Supose we are at x = 0 because that is our frame of reference
    // We compute the direction of the pol. at x = v * dt* N
    
    double lx = v * dt * N ;
    double deriv = 3 * coeffs[3] * lx*lx + 2 * coeffs[2]*lx + coeffs[1];
    double psil = atan(deriv);
    
    // We use a simple algoritm to set speed. That way in straight roads
    // we go fast and in bends we slow.
    
    ref_v = (max_v-min_v) * (1 - fabs(psil)*dec_factor/M_PI) + min_v;
    
    
    
    
    
    // number of independent variables
    // N timesteps == N - 1 actuations
    size_t n_vars = N * 6 + (N - 1) * 2;  // 2 son els actuators
    
    // Number of constraints
    size_t n_constraints = N * 6; // 6 son les variables
    
    
    // Initial value of the independent variables.
    // Should be 0 except for the initial values.
    Dvector vars(n_vars);
    for (int i = 0; i < n_vars; i++) {
        vars[i] = 0.0;
    }
    // Set the initial variable values
    vars[x_start] = x;
    vars[y_start] = y;
    vars[psi_start] = psi;
    vars[v_start] = v;
    vars[cte_start] = cte;
    vars[epsi_start] = epsi;
    
    // Lower and upper limits for x
    Dvector vars_lowerbound(n_vars);
    Dvector vars_upperbound(n_vars);
    
    // Set all non-actuators upper and lowerlimits
    // to the max negative and positive values.
    for (int i = 0; i < delta_start; i++) {
        vars_lowerbound[i] = -1.0e19;
        vars_upperbound[i] = 1.0e19;
    }
    
    // The upper and lower limits of delta are set to -25 and 25
    // degrees (values in radians).
    // NOTE: Feel free to change this to something else.
    for (int i = delta_start; i < a_start; i++) {
        vars_lowerbound[i] = -0.436332;
        vars_upperbound[i] = 0.436332;
    }
    
    // Acceleration/decceleration upper and lower limits.
    // NOTE: Feel free to change this to something else.
    for (int i = a_start; i < n_vars; i++) {
        vars_lowerbound[i] = -1.0;
        vars_upperbound[i] = 1.0;
    }
    
    // Lower and upper limits for constraints
    // All of these should be 0 except the initial
    // state indices.
    Dvector constraints_lowerbound(n_constraints);
    Dvector constraints_upperbound(n_constraints);
    for (int i = 0; i < n_constraints; i++) {
        constraints_lowerbound[i] = 0;
        constraints_upperbound[i] = 0;
    }
    constraints_lowerbound[x_start] = x;
    constraints_lowerbound[y_start] = y;
    constraints_lowerbound[psi_start] = psi;
    constraints_lowerbound[v_start] = v;
    constraints_lowerbound[cte_start] = cte;
    constraints_lowerbound[epsi_start] = epsi;
    
    constraints_upperbound[x_start] = x;
    constraints_upperbound[y_start] = y;
    constraints_upperbound[psi_start] = psi;
    constraints_upperbound[v_start] = v;
    constraints_upperbound[cte_start] = cte;
    constraints_upperbound[epsi_start] = epsi;
    
    // Object that computes objective and constraints
    FG_eval fg_eval(coeffs);
    
    // options
    std::string options;
    options += "Integer print_level  0\n";
    options += "Sparse  true        forward\n";
    options += "Sparse  true        reverse\n";
    // place to return solution
    //CppAD::ipopt::solve_result<Dvector> solution;
    
    
    // solve the problem
    CppAD::ipopt::solve<Dvector, FG_eval>(
                                          options, vars, vars_lowerbound, vars_upperbound, constraints_lowerbound,
                                          constraints_upperbound, fg_eval, solution);
    
    //
    // Check some of the solution values
    //
    bool ok = true;
    ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;
    
    auto cost = solution.obj_value;
    //std::cout << "Cost " << cost << std::endl;
    
    //
    // Incorporate latency
    //
    double latency = 0.1;
    int step = floor(latency/dt);
    return {solution.x[x_start + 1+step],   solution.x[y_start + 1+step],
        solution.x[psi_start + 1+step], solution.x[v_start + 1+step],
        solution.x[cte_start + 1+step], solution.x[epsi_start + 1+step],
        solution.x[delta_start+step],   solution.x[a_start+step], cost};
}
