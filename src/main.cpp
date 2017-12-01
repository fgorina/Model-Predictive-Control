#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "MPC.h"
#include "json.hpp"

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

/** transform transforms a point in particle coordinates tp one in map coordinates
 */

std::tuple<double, double>transformToMap(double x, double y, double x_car, double y_car, double sigma){
    
    double newx = x_car + cos(sigma)*x - sin(sigma)*y;
    double newy = y_car + sin(sigma)*x + cos(sigma)*y;
    
    return std::make_tuple(newx, newy);
}

std::tuple<double, double>transformToCar(double x, double y, double x_car, double y_car, double sigma){
    
    double dx = x - x_car;
    double dy = y - y_car;
    double carx= cos(sigma)*dx + sin(sigma)*dy;
    double cary = -sin(sigma)*dx + cos(sigma)*dy;
    
    return std::make_tuple(carx, cary);
}



// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
    auto found_null = s.find("null");
    auto b1 = s.find_first_of("[");
    auto b2 = s.rfind("}]");
    if (found_null != string::npos) {
        return "";
    } else if (b1 != string::npos && b2 != string::npos) {
        return s.substr(b1, b2 - b1 + 2);
    }
    return "";
}

// Evaluate a polynomial.
double polyeval(Eigen::VectorXd coeffs, double x) {
    double result = 0.0;
    for (int i = 0; i < coeffs.size(); i++) {
        result += coeffs[i] * pow(x, i);
    }
    return result;
}

// Fit a polynomial.
// Adapted from
// https://github.com/JuliaMath/Polynomials.jl/blob/master/src/Polynomials.jl#L676-L716
Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals,
                        int order) {
    assert(xvals.size() == yvals.size());
    assert(order >= 1 && order <= xvals.size() - 1);
    Eigen::MatrixXd A(xvals.size(), order + 1);
    
    for (int i = 0; i < xvals.size(); i++) {
        A(i, 0) = 1.0;
    }
    
    for (int j = 0; j < xvals.size(); j++) {
        for (int i = 0; i < order; i++) {
            A(j, i + 1) = A(j, i) * xvals(j);
        }
    }
    
    auto Q = A.householderQr();
    auto result = Q.solve(yvals);
    return result;
}

int main() {
    
    uWS::Hub h;
    
    // MPC is initialized here!
    MPC mpc;
    
    h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                       uWS::OpCode opCode) {
        // "42" at the start of the message means there's a websocket message event.
        // The 4 signifies a websocket message
        // The 2 signifies a websocket event
        string sdata = string(data).substr(0, length);
        if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
            //cout << sdata << endl;
            string s = hasData(sdata);
            if (s != "") {
                auto j = json::parse(s);
                string event = j[0].get<string>();
                if (event == "telemetry") {
                    // j[1] is the data JSON object
                    vector<double> ptsx = j[1]["ptsx"];
                    vector<double> ptsy = j[1]["ptsy"];
                    double px = j[1]["x"];
                    double py = j[1]["y"];
                    double psi = j[1]["psi"];
                    double v = j[1]["speed"];
                    /*
                     * TODO: Calculate steering angle and throttle using MPC.
                     *
                     * Both are in between [-1, 1].
                     *
                     */
                    
                    double steer_value;
                    double throttle_value;
                    
                    Eigen::VectorXd vptsx(ptsx.size());
                    Eigen::VectorXd vptsy(ptsy.size());
                    
                    // Apply car equations so we get the position after the delay
                    
                    // Convert waypoints to car coordinates. We do all math in car coordinates

                    for(int i = 0; i < ptsx.size(); i++){
                        
                        double newx;
                        double newy;
                        
                        std::tie(newx, newy) = transformToCar(ptsx[i], ptsy[i], px, py, psi);
                        
                        vptsx(i) = newx;
                        vptsy(i) = newy;
                    }
                    // Build a polynomium for the track
                    
                    auto coeffs = polyfit(vptsx, vptsy, 3);
                    
                    // Compoute initial errors. cte is computed as the difference between the track and car position at same x
		   // epsi is the difference in angles
                    
                    double cte = coeffs(0); //  double cte = polyeval(coeffs, px) - py;
                    
                    double epsi =  atan(coeffs(1));
                    
                    Eigen::VectorXd state(6);

		    // As we have converted to car coordinates, initial x, y and psi are 0.0, no need to compute anything

                    state << 0.0, 0.0, 0.0, v, cte, epsi;
                    
                    std::vector<double> x_vals = {state[0]};
                    std::vector<double> y_vals = {state[1]};
                    std::vector<double> psi_vals = {state[2]};
                    std::vector<double> v_vals = {state[3]};
                    std::vector<double> cte_vals = {state[4]};
                    std::vector<double> epsi_vals = {state[5]};
                    std::vector<double> delta_vals = {};
                    std::vector<double> a_vals = {};
                    
                    auto vars = mpc.Solve(state, coeffs);	// OK, solve th problem
                    
                    steer_value = -vars[6] / deg2rad(25);	// Get values back and scale
                    throttle_value = vars[7];
                    
                    json msgJson;

                    msgJson["steering_angle"] = steer_value;
                    msgJson["throttle"] = throttle_value;
                    
                    //Display the MPC predicted trajectory
                    vector<double> mpc_x_vals(ptsx.size());
                    vector<double> mpc_y_vals(ptsx.size());
                    
                    //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
                    // the points in the simulator are connected by a Green line
		    // We have moved solution to an instance variable so it is accesible

                    int steps = 15;
                    for(int i = 0; i < steps; i++){
                        mpc_x_vals.push_back(mpc.solution.x[0+i]);
                        mpc_y_vals.push_back(mpc.solution.x[steps+i]);
                    }

                    
                    msgJson["mpc_x"] = mpc_x_vals;
                    msgJson["mpc_y"] = mpc_y_vals;
                    
                    
                    // Now we compuite the track but instead of the original points
		    // We use the polynomia so it is smoother

                    vector<double> next_x_vals;
                    vector<double> next_y_vals;
                    
                    double lastx = vptsx(vptsx.size()-1);
                    double step_x = lastx / steps;
                    double current_x = 0.0;

                    for (int i = 0; i < steps; i++){
                        
                        next_x_vals.push_back(current_x);
                        next_y_vals.push_back(polyeval(coeffs, current_x));
                        current_x += step_x;
                    }
                    
                    msgJson["next_x"] = next_x_vals;
                    msgJson["next_y"] = next_y_vals;
                    
                    
                    auto msg = "42[\"steer\"," + msgJson.dump() + "]";
                    //std::cout << msg << std::endl;
                    // Latency
                    // The purpose is to mimic real driving conditions where
                    // the car does actuate the commands instantly.
                    //
                    // Feel free to play around with this value but should be to drive
                    // around the track with 100ms latency.
                    //
                    // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE
                    // SUBMITTING.
                    this_thread::sleep_for(chrono::milliseconds(100));
                    ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
                }
            } else {
                // Manual driving
                std::string msg = "42[\"manual\",{}]";
                ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
            }
        }
    });
    
    // We don't need this since we're not using HTTP but if it's removed the
    // program
    // doesn't compile :-(
    h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                       size_t, size_t) {
        const std::string s = "<h1>Hello world!</h1>";
        if (req.getUrl().valueLength == 1) {
            res->end(s.data(), s.length());
        } else {
            // i guess this should be done more gracefully?
            res->end(nullptr, 0);
        }
    });
    
    h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        std::cout << "Connected!!!" << std::endl;
    });
    
    h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                           char *message, size_t length) {
        ws.close();
        std::cout << "Disconnected" << std::endl;
    });
    
    int port = 4567;
    if (h.listen(port)) {
        std::cout << "Listening to port " << port << std::endl;
    } else {
        std::cerr << "Failed to listen to port" << std::endl;
        return -1;
    }
    h.run();
}
