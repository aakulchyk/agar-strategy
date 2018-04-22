#include "../nlohmann/json.hpp"
#include "utils.h"


#include <fstream>
#include <unistd.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <exception>
#include <time.h>
#include <map>
#include <vector>

using namespace std;
using namespace Utils;
using json = nlohmann::json;

//#define WRITE_LOGS

struct Strategy {

    enum State {INITIAL, FIND_ENEMY, FIND_FOOD, FLEE, CONTINUE_MOVEMENT, FIND_DIRECTION, CONSIDER_SPLITTING, GATHER, ACT};
    enum Target {FOOD, ENEMY, VIRUS, FLEEING, NO_TARGET};

    json mine;
    json objects;
    json config;

    Target current_purpose;
    json current_target = json({});
    json prev_target = json({});
    std::map<Point, bool> point_available;
    json virus_list;
    json food_list;
    json enemy_list;
    std::ostringstream sstream;

    int chasing_count = 0;
    Point flee_point;
    int flee_count = 0;
    int just_splitted = 0;

    void write_err_log(string s) {
        ofstream file;
        file.open("/dev/shm/agar-err.log", std::ios_base::app);
        file << s << endl << flush;
        file.close();
    }

    void run() {
        string data;
        cin >> data;

#ifdef WRITE_LOGS
        remove("/dev/shm/agar-world.json");
        remove("/dev/shm/agar-cmd.json");

		ofstream file;
		file.open("/dev/shm/agar-config.json");
		file << data << flush;
		file.close();
		sync();
#endif
		auto config = json::parse(data);
		while (true) {
			cin >> data;

#ifdef WRITE_LOGS
            // write world to log
            file.open("/dev/shm/agar-world.json", std::ios_base::app);
			file << data << endl << flush;
			file.close();
#endif
			auto parsed = json::parse(data);
			auto command = on_tick(parsed, config);
			cout << command.dump() << endl << flush;
#ifdef WRITE_LOGS
            // write commands to log
            file.open("/dev/shm/agar-cmd.json", std::ios_base::app);
			file << command.dump() << endl << flush;
			file.close();
			sync();
#endif
		}
	}

    json on_tick(const json &data, const json &conf) {
        double tx = 0, ty = 0;
        State state = INITIAL;
        sstream.str("");
        sstream.clear();

        try {
            // update current game date with new tick json
            mine = data["Mine"];
            objects = data["Objects"];
            config = conf;

            if (mine.empty())
                return {{"X", 0}, {"Y", 0}, {"Debug", "Died"}};

            current_purpose = NO_TARGET;
            if (!find_existing_object(current_target, objects)) {
                prev_target = current_target;
                current_target = json({});
            }

            food_list = find_objects(objects, "F");
            enemy_list = find_objects(objects, "P");
            virus_list = clear_unvisible(mine, find_objects(objects, "V"));

            // sort player fragments by mass in descending order
            std::sort(mine.rbegin(), mine.rend(), [=](const json &p, const json &q) {
                return (double)p["M"] < (double)q["M"];
            });

            // sort enemies by mass in descending order
            std::sort(enemy_list.rbegin(), enemy_list.rend(), [=](const json &p, const json &q) {
                return (double)p["M"] < (double)q["M"];
            });

            // init target coordinates
            tx = (double)config["GAME_WIDTH"] / 2, ty = (double)config["GAME_HEIGHT"] / 2;
            auto split = json({"Split", false});

            double inertion = config["INERTION_FACTOR"];

            if (flee_count > 0)
                state = FLEE;

            while (state != ACT) {
                switch (state) {
                    case INITIAL: {
                        auto pred = [](const json &p) {
                            return p.find("TTF") != p.end();
                        };
                        if (mine.size() > 1 && find_if(mine.begin(), mine.end(), pred) == mine.end() )
                                state = GATHER;
                            else
                                state = FIND_ENEMY;
                        break;
                    }
                    case GATHER: {
                        state = gather(tx, ty);
                        break;
                    }
                    case FLEE: {
                        state = flee(tx, ty);
                        break;
                    }

                    case FIND_ENEMY: {
                         state = find_enemy(tx, ty);
                         break;
                    }
                    case FIND_FOOD: {
                        state = find_food_experimental(tx, ty);
                        break;
                    }
                    case CONTINUE_MOVEMENT: {
                        state = continue_movement(tx, ty);
                        break;
                    }

                    case FIND_DIRECTION: {
                        state = find_direction(tx, ty);
                        break;
                    }

                    case CONSIDER_SPLITTING: {
                        if (consider_splitting(mine, tx, ty, config, objects))
                            split = {"Split", true};
                        state = ACT;
                        break;
                    }

                    case ACT:
                        break;
                }
            }

            if (current_purpose == FOOD || current_purpose == ENEMY)
                update_target_coord_if_inertion(tx, ty, biggest_player(mine), inertion, sstream);

            if (current_purpose != FLEEING) {
                if (!prev_target.empty() && prev_target["T"] == "P")
                    prev_target = json({});
                flee_count = 0;
            }

            for (auto &p : mine)
                sstream << std::setw(4) << " (S: " << player_speed(p) << ") ";

            sstream << " P: " << current_purpose;

            return {{"X", tx}, {"Y", ty}, {"Debug", sstream.str()}, split};
        }
        catch (std::exception &e) {
            //std::ostringstream sstream;
            sstream << " STATE: " << state << " tx/y " << tx << " " << ty << " current_purpose " << current_purpose;
            //std::string copyOfStr = stringStream.str();
            //write_err_log("on_tick " + string(e.what()) + sstream.str());
            throw std::runtime_error("on_tick " + string(e.what()) + sstream.str());
            exit(1);
        }
        catch (...) {
            sstream << "STATE: "<< state << " tx/y " << tx << " " << ty << " current_purpose " << current_purpose;
            write_err_log("OTHER EXCEPTION " + sstream.str());
            exit(1);
        }
    }


    State gather(double &tx, double &ty)
    {
        // find center of masses & try to gather
        Point c = find_center_point(mine);
        if (consider_moving(mine, c.x, c.y, config)) {
            sstream << "Gather ";
            tx = c.x; ty = c.y;
            return ACT;
        }
        else
            return FIND_ENEMY;
    }

    template <class T>
    void try_random_flee(const T &player, Point e, Point &safest, const T &config, bool ignore_viruses) {
        /*Point p_curr(player["X"], player["Y"]);
        auto range = 1 * (double)player["R"];
        auto angle = (rand() % 360) * M_PI / 180.;

        Point pot = new_point_by_vector(p_curr, angle, range);
        */
        auto rf = 2 * (double)player["R"];
        auto rx = rand() % (2 * (int)(rf+1)) - rf + 1;
        auto ry = rand() % (2 * (int)(rf+1)) - rf + 1;

        double ptx = (double)player["X"] + rx;
        double pty = (double)player["Y"] + ry;
        Point pot(ptx, pty);
        if (dist(pot, e) > dist(safest, e) && consider_moving(mine, pot.x, pot.y, config, ignore_viruses))
            safest = pot;
    }

    // FLEE
    State flee(double &tx, double &ty) {
        if ((current_target.empty() || current_target["T"] != "P") && flee_count <= 0)
            return CONTINUE_MOVEMENT;

        current_purpose = FLEEING;
        auto last = mine.size()-1;

        try {
            bool didnt_stuck = Point(mine[0]["X"], mine[0]["Y"]) != flee_point;
            bool can_move = consider_moving(mine, flee_point.x, flee_point.y, config);
            sstream << "[COND: " << flee_count << " " << didnt_stuck << " " << can_move << " " << flee_point << "] ";
            if (flee_count > 0  &&  didnt_stuck && can_move) {
                flee_count--;
            }
            else {
                Point e;

                if (!current_target.empty())
                    e.set(current_target["X"], current_target["Y"]);
                /*else if (!prev_target.empty())
                    e.set(prev_target["X"], prev_target["Y"]);*/
                else {
                    flee_count--;
                    sstream << ":Lost targ: ";
                    return CONTINUE_MOVEMENT;
                }

                auto player = mine[0];
                double px = player["X"], py = player["Y"];
                Point p(px, py);
                Point safest_point = e; // initialize

                auto angle = line_angle(Line(p, e));
                auto range = 1 * (double)player["R"];

                Point pot = new_point_by_vector(p, angle, range);

                if (enemy_list.size() < 2 && mine.size() < 2 && consider_moving(mine, pot.x, pot.y, config)) {
                    flee_point = pot;
                    sstream << "One {" << e << "}";
                }
                else {
                    sstream << "(Try escape) ";
                    for (auto &player : mine) {
                        int count = 30;
                        while (--count) try_random_flee(player, e, safest_point, config, false);
                    }

                    if (safest_point == e) {
                        sstream << "(Try ignore V) ";
                        // try ignoring viruses
                        for (auto &player : mine) {
                            int count = 30;
                            while (--count) try_random_flee(player, e, safest_point, config, true);
                        }

                        if (safest_point == e) {
                            flee_count = 0;
                            sstream << "didnt find :(";
                            return FIND_DIRECTION;
                        }
                    }
                }

                flee_point = safest_point;
                flee_count = 20;
            }
        }
        catch (...) {
            throw std::runtime_error("flee: find point ");
        }

        tx = flee_point.x; ty = flee_point.y;
        auto id = current_target.empty() ? "unknown" : current_target["Id"];
        sstream << "Flee from " << id << " to " << flee_point;
        return CONSIDER_SPLITTING;
    }


    State find_enemy(double &tx, double &ty) {

        // find enemy
        // TODO: memorize where seen enemy last time
        auto nearest_enemy = json({});
        auto biggest_enemy = enemy_list[0];
        auto min_e_dist = 0;
        json closest_p = mine[0]; // initialize
        try {
            for (auto &e: enemy_list) {
                if (e.empty()) continue;
                json curr_closest_p;
                double curr_e_dist = mine_obj_dist(mine, e, curr_closest_p);
                if (!nearest_enemy.empty() && curr_e_dist > min_e_dist)
                    continue;
                nearest_enemy = e;
                min_e_dist = curr_e_dist;

                if (!curr_closest_p.empty())
                    closest_p = curr_closest_p;
            }
        }
        catch (...) {
            throw std::runtime_error("find_enemy 1 ");
        }

        try {
            auto pm = (double)closest_p["M"], pr = (double)closest_p["R"];
            if (!biggest_enemy.empty()) {
                auto em = (double)biggest_enemy["M"];

                // if enemy is harmful and close
                if (em > pm*1.1/* && min_e_dist < pr*3.5*/) {
                    current_target = biggest_enemy;
                    return FLEE;
                } // if enemy cannot be eaten (too big or unavailable)
            }

            if (!nearest_enemy.empty()) {
                auto em = (double)nearest_enemy["M"],
                    ex = (double)nearest_enemy["X"],
                    ey = (double)nearest_enemy["Y"];
                if (pm < em*1.22 || !consider_moving(mine, ex, ey, config))
                    nearest_enemy = json({}); // don't consider as target
            }
        }
        catch (...) {
            throw std::runtime_error(" find_enemy 2");
        }

        if (!nearest_enemy.empty()) {
            current_target = nearest_enemy;
            current_purpose = ENEMY;
            sstream << "Try eat enemy ";
            tx = current_target["X"];
            ty = current_target["Y"];
            return CONSIDER_SPLITTING;
        }
        else {
            return FIND_FOOD;
        }
    }

    State find_food_experimental(double &tx, double &ty) {

        json better_food;
        bool chasing = false;
        if (!current_target.empty() && current_target["T"] == "F"
                && consider_moving(mine, current_target["X"], current_target["Y"], config) && ++chasing_count < 300) {
            sstream << "chase " << chasing_count << " ";
            chasing = true;
        }
        else {
            sstream << "stop ";
            chasing_count = 0;
            current_target = json({});
        }

        if (!chasing) {
            // find densest food cluster
            double pr = biggest_player(mine)["R"];

            using IntPair = pair<int, int>;

            vector<IntPair> food_dense_map(food_list.size());

            try {
                for (int i = 0; i < food_list.size(); i++) {
                    food_dense_map[i].first = i;
                    food_dense_map[i].second = 0;
                }
            }
            catch (...) {
                throw std::runtime_error("find_food_experimental 0 ");
            }

            try {
                for (int i = 0; i < food_list.size(); i++) {
                    if (food_list[i].empty()) continue;
                    double fx = food_list[i]["X"], fy = food_list[i]["Y"];
                    Point p(fx, fy);
                    for (int j = i+1; j < food_list.size(); j++) {
                        double fjx = food_list[i]["X"], fjy = food_list[i]["Y"];
                        Point q(fjx, fjy);
                        if (dist(p, q) < pr) {
                            food_dense_map[i].second++;
                            food_dense_map[j].second++;
                        }
                    }
                }
            }
            catch (...) {
                throw std::runtime_error("find_food_experimental 1 size:" + to_string(food_list.size()));
            }

            try {
                // sort by density
                std::sort(food_dense_map.rbegin(), food_dense_map.rend(), [=](const IntPair& p1, const IntPair& p2) {
                    return p1.second < p2.second;
                });
            }
            catch (...) {
                throw std::runtime_error("find_food_experimental 2 ");
            }

            try {
                for (auto &f_i : food_dense_map) {
                    if (food_list[f_i.first].empty()) continue;
                    auto f = food_list[f_i.first];
                    auto fx = (double)f["X"], fy = (double)f["Y"];
                    if (consider_moving(mine, fx, fy, config)) {
                        better_food = f;
                        break;
                    }
                }
            }
            catch (...) {
                throw std::runtime_error("find_food_experimental 3 ");
            }

            if (!better_food.empty()) {
                chasing_count = 0;
                sstream << "new food ";
                current_target = better_food;
            }
        }

        if (!current_target.empty()) {
            current_purpose = FOOD;
            tx = current_target["X"];
            ty = current_target["Y"];
            return CONSIDER_SPLITTING;
        }
        else
            return CONTINUE_MOVEMENT;
    }

    State find_food(double &tx, double &ty) {

        // find nearest food
        json nearest_food;

        try {
            double min_f_dist = 0, min_f_angle = 0;
            for (auto &f: food_list) {
                if (f.empty()) continue;
                auto fx = (double)f["X"], fy = (double)f["Y"];
                double curr_f_dist = mine_obj_dist(mine, f);
                //double curr_f_angle = player_obj_angle(biggest_player(mine), f);
                if (!nearest_food.empty() && curr_f_dist > min_f_dist/* && curr_f_angle > min_f_angle*/)
                    continue;

                if (consider_moving(mine, fx, fy, config)) {
                    nearest_food = f;
                    min_f_dist = curr_f_dist;
                }
            }


            // when player is heave and clumsy, better not to chose, just take anything!
            bool chasing = false;

            //auto pm = (double)biggest_player(mine)["M"];
            //if (pm > 300) {
                if (!current_target.empty() && current_target["T"] == "F"
                        && consider_moving(mine, current_target["X"], current_target["Y"], config)) {
                    if (++chasing_count < 300) {
                        nearest_food = current_target;
                        sstream << "Chase same food ";
                        current_purpose = FOOD;
                        chasing = true;
                    }
                    else {
                        chasing_count = 0;
                        current_target = json({});
                    }
                }
            //}

            if (!chasing && !nearest_food.empty()) {
                current_purpose = FOOD;
                sstream << "Found new food ";
                current_target = nearest_food;
            }

            if (!current_target.empty()) {
                tx = current_target["X"];
                ty = current_target["Y"];
                return CONSIDER_SPLITTING;
            }
            else
                return CONTINUE_MOVEMENT;

        }
        catch (...) {
            throw std::runtime_error("find_food ");
        }
    }


    State continue_movement(double &tx, double &ty) {
        try {
            auto p = mine[0];
            double px = p["X"], py = p["Y"];
            double sx = p["SX"], sy = p["SY"];
            double speed = player_speed(p);
            if (speed > 0.2) {
                tx = px + sx;
                ty = py + sy;

                if (px < (double)config["GAME_WIDTH"] / 3)
                    tx++;
                else
                if (px > (double)config["GAME_WIDTH"] * 2 / 3)
                    tx--;

                if (py < (double)config["GAME_HEIGHT"] / 3)
                    ty++;
                else
                if (py > (double)config["GAME_HEIGHT"] * 2 / 3)
                    ty--;

                if (consider_moving(mine, tx, ty, config)) {
                    sstream << "Continue ";
                    return CONSIDER_SPLITTING;
                }
            }

            return FIND_DIRECTION;

        }
        catch (...) {
            throw std::runtime_error("continue_movement ");
        }
    }


    State find_direction(double &tx, double &ty) {
        try {
            auto count = 20;
            do {

                /*Point p_curr(player["X"], player["Y"]);
                auto range = 1 * (double)player["R"];
                auto angle = (rand() % 360) * M_PI / 180.;

                Point pot = new_point_by_vector(p_curr, angle, range);
                */

                auto p = biggest_player(mine);
                double px = p["X"], py = p["Y"], rf = p["R"];
                auto rx = rand() % (2 * (int)(rf+1)) - rf + 1;
                auto ry = rand() % (2 * (int)(rf+1)) - rf + 1;

                if (px < (double)config["GAME_WIDTH"] / 4)
                    rx++;
                else
                if (px > (double)config["GAME_WIDTH"] * 3 / 4)
                    rx--;

                if (py < (double)config["GAME_HEIGHT"] / 4)
                    ry++;
                else
                if (py > (double)config["GAME_HEIGHT"] * 3 / 4)
                    ry--;

                tx = (double)px + rx;
                ty = (double)py + ry;
            } while (!consider_moving(mine, tx, ty, config) && --count);

            if (count == 0) {
                tx = (double)config["GAME_WIDTH"] / 2;
                ty = (double)config["GAME_HEIGHT"] / 2;
            }

            sstream << "Random ";
            return CONSIDER_SPLITTING;
        }
        catch (...) {
            throw std::runtime_error("find_direction ");
        }
    }


    template <class T>
    double player_speed(const T& p) {
        double sx = p["SX"], sy = p["SY"];
        return sqrt(sx*sx + sy*sy);
    }


    template <class T>
    json biggest_player(const T& mine) {
        /*json biggest = mine[0];
        for (auto &p : mine) {
            if ((double)p["M"] > (double)biggest["M"])
                biggest = p;
        }*/
        return mine[0];
    }


    template <class T>
    double player_obj_angle(const T& p, const T& obj) {
        auto px = (double)p["X"], py = (double)p["Y"],
            sx = (double)p["SX"], sy = (double)p["SY"];

        auto ox = (double)obj["X"], oy = (double)obj["Y"];

        Point p_curr(px, py);
        Point p_pot(px+sx, py+sy);
        Point p_targ(ox, oy);

        Line l1(p_curr, p_pot);
        Line l2(p_curr, p_targ);

        return two_lines_angle(l1, l2);
    }

    template <class T>
    void update_target_coord_if_inertion(double &tx, double &ty, const T& p, double inertion, std::ostringstream &stream) {
        auto px = (double)p["X"], py = (double)p["Y"],
            sx = (double)p["SX"], sy = (double)p["SY"],
            pr = (double)p["R"]*1.1, pm = (double)p["M"];

        Point p_curr(px, py);
        Point p_pot(px+sx, py+sy);
        Point p_targ(tx, ty);

        Line l1(p_curr, p_pot);
        Line l2(p_curr, p_targ);

        double d = dist(p_curr, p_targ);
        double luft_angle = asin(pr / d);

        double speed = player_speed(p);

        // angle
        double pot_angle = line_angle(l1);
        double targ_angle = line_angle(l2);

        double delta_angle = two_lines_angle(l1, l2);

        double new_plus_delta_angle = delta_angle * speed / inertion;

        if (new_plus_delta_angle > M_PI / 2)
            new_plus_delta_angle = 0;
        double new_angle = targ_angle + new_plus_delta_angle;

        Point new_p = new_point_by_vector(p_curr, new_angle, d);
        tx = new_p.x, ty = new_p.y;
    }

    template <class T>
    Point find_center_point(const T& mine) {
        try {
            auto count = 0;
            double tx = 0, ty = 0;
            for (auto &p : mine) {
                tx += (double)p["X"];
                ty += (double)p["Y"];
                count++;
            }

            tx /= count; ty /= count;
            return Point(tx, ty);
        }
        catch (exception &e) {
            write_err_log("find_center_point " + string(e.what()));
            exit(1);
        }
    }

    template <class T>
    bool find_existing_object(const T& target, const T& objects) {
        try {
            if (target.empty()) return false;
            for (auto &o: objects) {
                if (o["T"] == target["T"]) {
                    if (o["T"] == "F" && double_equal(o["X"], target["X"]) && double_equal(o["Y"], target["Y"]))
                        return true;
                    else if (o["T"] == "P" && o["Id"] == target["Id"])
                        return true;
                }

            }

            return false;
        }
        catch (exception &e) {
            write_err_log("find_existing_object " + string(e.what()));
            exit(1);
        }
    }

    template <class T>
    double mine_obj_dist(const T& mine, const T& obj, T& p_closest) {
        try {

            double min_dist = std::numeric_limits<double>::max();

            for (auto &player : mine) {
                double curr_dist = Utils::dist(Point(player["X"], player["Y"]), Point(obj["X"], obj["Y"]));
                if (curr_dist < min_dist) {
                    min_dist = curr_dist;
                    p_closest = player;
                }
            }

            return min_dist;
        }
        catch (exception &e)
        {
            write_err_log("obj_distance " + string(e.what()));
            exit(1);
        }
    }

    template <class T>
    double mine_obj_dist(const T& mine, const T& obj) {
        T unused;
        return mine_obj_dist(mine, obj, unused);
    }


    template <class T>
    bool consider_moving(const T& mine, double x, double y, const T& config, bool ignore_viruses = false) {
        try {
            for (auto &player : mine) {
                // player coordinates
                auto px = (double)player["X"], py = (double)player["Y"],
                    pr = (double)player["R"]*1.01, pm = (double)player["M"];

                Point p_curr(px, py);
                Point p_targ(x, y);
                Circle c_targ(p_targ, pr);
                Circle c_curr(p_curr, pr);
                auto pr2 = pr/2;
                // map boundaries
                if (x-pr2 < 0 || y-pr2 < 0 || x+pr2 > (double)config["GAME_WIDTH"]-1 || y+pr2 > (double)config["GAME_HEIGHT"]-1) {
                    //sstream << "_m.";
                    return false;
                }

                // check for viruses
                if (!ignore_viruses && pm > 120. && virus_on_way(player, p_targ, config, true)) {
                    //sstream << "_v.";
                    return false;
                }

                // check for bigger enemies
                for (auto &e: enemy_list) {
                    if (e.empty()) continue;
                    double ex = e["X"], ey = e["Y"], em = e["M"], er = e["R"];
                    Circle c_enemy(Point(ex, ey), er);
                    bool mass_danger = em > pm*1.15;
                    bool targ_intersects = intersect(c_targ, c_enemy);
                    bool curr_intersects = intersect(c_curr, c_enemy);
                    bool line_crosses = cross_line_circle(p_curr, p_targ, c_enemy);
                    bool targ_closer_than_curr = dist(p_targ, c_enemy.c) < dist(p_curr, c_enemy.c);
                    bool path_intersects = line_crosses && targ_closer_than_curr;
                    if ( mass_danger && (targ_intersects || (!curr_intersects && path_intersects)) ) {
                        //sstream << "_e" << targ_intersects << (!curr_intersects && path_intersects) << ".";
                        return false;
                    }
                }
            }
            return true;
        }
        catch (exception &e)
        {
            write_err_log("consider_moving " + string(e.what()));
            exit(1);
        }
    }

    template <class T>
    bool virus_on_way(const T& player, Point p_targ, const T& config, bool onlyVisible) {
        auto px = (double)player["X"], py = (double)player["Y"],
            pr = (double)player["R"]*1.01;

        Point p_curr(px, py);
        Circle tc(p_targ, pr);

        auto vr = (double)config["VIRUS_RADIUS"] * 1.01;
        for (auto &v: virus_list) {
            if (v.empty()) continue;
            Circle pc(p_curr, pr);
            auto vx = v["X"], vy = v["Y"];
            Circle vc(Point(vx, vy), vr);

            Point vec = p_targ - p_curr;
            auto dist_curr = dist(p_curr, vc.c);
            auto dist_targ = dist(p_targ, vc.c);

            bool additionalCondition = onlyVisible ? intersect(tc, vc) || dist(tc.c, tc.c) < pr : true;

            if (check_cirlce_path_intersection_with_circle(pc, vec, vc) && dist_targ < dist_curr && additionalCondition)
                return true;
        }

        return false;
    }

    template <class T>
    bool consider_splitting(const T& mine, double x, double y, const T& config, const T& objects) {
        try {
            bool maybe = false;

            if (just_splitted > 0) {
                just_splitted--;
                return false;
            }

            if (mine.size() > 4 && current_purpose != FLEEING)
                return false;

            for (auto &p : mine) {
                auto px = (double)p["X"], py = (double)p["Y"],
                    pr = (double)p["R"], pm = (double)p["M"],
                    sx = (double)p["SX"], sy = (double)p["SY"];

                if (pm < 120.)
                    continue;

                // check for visures
                Point curr(px, py);
                Point pot(px + sx, py + sy);
                if (pm/2 > 120. && virus_on_way(p, pot, config, false) && current_purpose != FLEEING)
                    return false;

                for (auto &e: enemy_list) {
                    if (e.empty()) continue;
                    auto ex = (double)e["X"], ey = (double)e["Y"],
                        er = (double)e["R"], em = (double)e["M"];

                    // if fleeing desperately (enemy starts to eat us) - split and save something! ignore viruses
                    Point ep(ex, ey);
                    if (current_purpose == FLEEING && intersect(px, py, pr, ex, ey, er) && dist(pot, ep) > dist(curr, ep))
                        return true;

                    // if bigger enemies nearby - don't split
                    if (pm/2 < em/1.2) {
                        return false;
                    }

                    // if some weak monster on the way - contsider splitting
                    // but check for other enemies anyway
                    if (pm/2 > em*1.2) {
                        Circle c(Point(ex, ey), er);
                        auto curr_dist = dist(curr, c.c);
                        if (cross_line_circle(curr, pot, c)  &&  curr_dist < pr * 3.7  &&  dist(pot, c.c) < curr_dist)
                            maybe = true;
                    }
                }

                // split if very big and no enemies nearby
                if (mine.size() < 3 && (enemy_list.empty() || enemy_list[0].empty()) && pm > 400 - 2*sqrt(pm)/(double)config["INERTION_FACTOR"])
                    maybe = true;
            }

            if (maybe)
                just_splitted = 200;

            return maybe;
        }
        catch (exception &e)
        {
            write_err_log("consider_splitting " + string(e.what()));
            exit(1);
        }
    }

    template <class T>
    double obj_dist(const T& obj1, const T& obj2) {
        Point o1(obj1["X"], obj1["Y"]);
        Point o2(obj2["X"], obj2["Y"]);
        return dist(o1, o2);
    }

    template <class T>
    bool player_sees_obj(const T& player, const T& obj) {
        auto pr = (double)player["R"]*1.1;
        // set radius a bit more than actial seeing to be able see more viruses
        return obj_dist(player, obj) < 6 * pr;
    }

    template <class T>
    json find_objects(const T &objects, const string &type) {
        try {
            json result = objects;

            for (auto it = result.begin(); it != result.end();) {
                if ((*it)["T"] != type)
                    it = result.erase(it);
                else
                    ++it;
            }

            return result;
        }
        catch (exception &e)
        {
            write_err_log("find_objects " + string(e.what()));
            exit(1);
        }
    }

    template <class T>
    json clear_unvisible(const T &mine, const T &objects) {
        try {
            json result = objects;

            for (auto it = result.begin(); it != result.end();) {
                bool visible = false;
                for (auto &p : mine) {
                    if (player_sees_obj(p, (*it)))
                        visible = true;
                }

                if (!visible)
                    it = result.erase(it);
                else
                    ++it;
            }

            return result;
        }
        catch (exception &e)
        {
            write_err_log("find_objects " + string(e.what()));
            exit(1);
        }
    }
};

#include "test_utils.h"

int main() {
    srand (time(NULL));
    // TEST
    test_utils();

	Strategy strategy;
	strategy.run();
	return 0;
}
