#pragma once

#include <iostream>
#include <assert.h>

#include "utils.h"


void test_dist() {
    Point p1, p2;

    p1.set(0, 0);
    p2.set(100, 0);
    assert( double_equal(dist(p1, p2), 100));

    p1.set(100, 100);
    p2.set(50, 50);
    assert( double_equal(dist(p1, p2), 70.71));
}

void test_projection() {
    Point a, b, c, r;

    a.set(0, 0);
    a.set(100, 0);

    c.set(50, 20);

    r = projection(a, b, c);

    assert( double_equal(r.x, 50.));
    assert( double_equal(r.y, 0.));

    a.set(5, 5);
    b.set(25, 25);
    c.set(5, 25);

    r = projection(a, b, c);

    assert( double_equal(r.x, 15.));
    assert( double_equal(r.y, 15.));
}

void test_cross_line_circle() {

    Point p1, p2;
    Circle c;

    // intersection
    p1.set(0., 0.);
    p2.set(100., 0.);
    c.set(Point(50, 10), 20.);
    assert( cross_line_circle(p1, p2, c) == true);

    // no intersection
    c.set(Point(50, 40), 20.);
    assert( cross_line_circle(p1, p2, c) == false);

    // intersection
    p1.set(20., 20.);
    p2.set(40., 40.);
    c.set(Point(60, 60), 10.);
    assert( cross_line_circle(p1, p2, c) == true);

    // intersection
    c.set(Point(0, 0), 10.);
    assert( cross_line_circle(p1, p2, c) == true);


    // no intersection
    c.set(Point(80, 20), 10.);
    assert( cross_line_circle(p1, p2, c) == false);
}

void test_check_cirlce_path_intersection_with_circle() {
    Circle c1, c2;
    Point vec;

    c1.set(Point(10, 10), 5);
    c2.set(Point(30, 10), 5);

    vec.set(1., 0);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(20., -7.5);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(20., 7.5);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(15, 10);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    vec.set(15, -10);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    c1.set(Point(30, 10), 5);
    c2.set(Point(10, 10), 5);

    vec.set(-1., 0);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(-20., -7.5);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(-20., 7.5);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(-15, 10);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    vec.set(-15, -10);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    c1.set(Point(10, 10), 5);
    c2.set(Point(10, 30), 5);

    vec.set(0, 1);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(-7.5, 20);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(7.5, 20);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(10, 15);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    vec.set(-10, 15);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    c1.set(Point(10, 30), 5);
    c2.set(Point(10, 10), 5);

    vec.set(0, -1);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(-7.5, -20);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(7.5, -20);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == true);

    vec.set(10, -15);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

    vec.set(-10, -15);
    assert(check_cirlce_path_intersection_with_circle(c1, vec, c2) == false);

}

void test_line_angle() {
    Line l;
    double res;

    l.p.set(10, 10);
    l.q.set(11,10);
    //std::cout << line_angle(l) << std::endl;
    assert(double_equal(line_angle(l), 0));

    // 90
    l.p.set(10, 10);
    l.q.set(10, 11);
    //std::cout << line_angle(l) << std::endl;
    assert(double_equal(line_angle(l), M_PI / 2));

    // 180
    l.p.set(10, 10);
    l.q.set(9,10);
    //std::cout << line_angle(l) << std::endl;
    assert(double_equal(line_angle(l), M_PI));

    // 270
    l.p.set(10, 10);
    l.q.set(10,9);
    //std::cout << line_angle(l) << std::endl;
    assert(double_equal(line_angle(l), 3 * M_PI / 2));

}

void test_new_point_by_vector() {
    Point p;
    double angle, dist = 1.;
    Point val;

    p.set(300, 300);

    angle = 0;//M_PI/2;
    val = new_point_by_vector(p, angle, dist);
    assert(val == Point(301, 300));

    angle = M_PI/2;
    val = new_point_by_vector(p, angle, dist);
    assert(val == Point(300, 301));

    angle = M_PI;
    val = new_point_by_vector(p, angle, dist);
    assert(val == Point(299, 300));

    angle = 3*M_PI/2;
    val = new_point_by_vector(p, angle, dist);
    assert(val == Point(300, 299));

}

void test_utils() {
    test_dist();
    test_projection();
    test_cross_line_circle();
    test_check_cirlce_path_intersection_with_circle();
    test_line_angle();
    test_new_point_by_vector();
}

