#pragma once

#include <cmath>
#include <limits>
#include <iostream>


using namespace std;

namespace Utils {

    const double EPS = 0.3;
    bool double_equal(double p, double q) {
        return (abs (p - q) <= EPS);
    }

    struct Point {
        double x, y;
        Point(double px = 0.0, double py = 0.0) : x (px), y(py) {}
        void set(double px, double py) {
            x = px; y = py;
        }

        bool operator<(const Point &that) {
            if (double_equal(y, that.y))
                return x < that.x;
            else
                return y < that.y;
        }

        bool operator==(const Point &that) {
            return double_equal(x, that.x) && double_equal(y, that.y);
        }

        bool operator!=(const Point &that) {
            return false == (double_equal(x, that.x) && double_equal(y, that.y));
        }

        friend std::ostream& operator<<(std::ostream &out, Point p) {
            out << "Pt:(" << p.x << ", " << p.y << ")";
            return out;
        }

    };

    Point operator+(const Point &p, const Point &q) {
        return Point(p.x + q.x, p.y + q.y);
    }

    Point operator-(const Point &p, const Point &q) {
        return Point(p.x - q.x, p.y - q.y);
    }

    struct Circle {
        Point c;
        double r;
        Circle(Point p = Point(), double rad = 0.0) : c(p), r(rad) {}
        void set(const Point &p, double rad) {
            c = p; r = rad;
        }
    };

    struct LineSegment {
        Point a, b;
        LineSegment(Point pa, Point pb) : a(pa), b(pb) {}
    };

    struct Line {
        double k, b;
        Point p, q;
        void from_points(double x1, double y1, double x2, double y2) {
            if (!double_equal(x2-x1, 0)) {
                k = (y2-y1) / (x2-x1); //угловой Коэфицент прямой
                b = (x2*y1-x1*y2)/(x2-x1); //смещение прямой
            }
            else
                k = std::numeric_limits<double>::max(), b = 0;
        }

        Line(double lk = 0, double lb = 0) : k(lk), b(lb), p(0), q(0) {}
        Line(Point p1, Point p2) : p(p1), q(p2) {
            from_points(p.x, p.y, q.x, q.y);
        }
    };

    double dist(double pa,double pb,double qa,double qb) {
        return sqrt((qa - pa)*(qa - pa) + (qb - pb)*(qb - pb));
    }

    double dist(const Point &p, const Point &q) {
        return sqrt((q.x - p.x)*(q.x - p.x) + (q.y - p.y)*(q.y - p.y));
    }


    Point projection (Point A, Point B, Point C) // проекция точки С на прямую AB
    {
        double x = B.y - A.y; //x и y - координаты вектора, перпендикулярного к AB
        double y = A.x - B.x;
        double L = (A.x*B.y - B.x*A.y + A.y*C.x - B.y*C.x + B.x*C.y - A.x*C.y)/(x*(B.y - A.y) + y*(A.x - B.x));
        Point H;
        H.x = C.x + x * L;
        H.y = C.y + y * L;
        return H;
    }


    bool cross_line_circle (const Point &p1, const Point &p2, const Circle &c /*point &p1, point &p2*/)
    {
        // проекция центра окружности на прямую
        Point p = projection (p1, p2, c.c);
        // сколько всего решений?
        int flag = 0;
        double d = dist(c.c, p);
        if (double_equal(d, c.r))
            return true;//flag = 1;
        else
            if (c.r > d) return true;//flag = 2;
            else
                return false;

        // находим расстояние от проекции до точек пересечения
        //double k = sqrt (c.r * c.r - d * d);
        //double t = dist (point (0, 0), point (l.b, - l.a));
        // добавляем к проекции векторы направленные к точкам пеерсечения
        //p1 = add_vector (p, point (0, 0), point (- l.b, l.a), k / t);
        //p2 = add_vector (p, point (0, 0), point (l.b, - l.a), k / t);

        //return flag;
    }

    bool commonSectionCircle    (   double x1, double y1, double x2, double y2,
                                    double xC, double yC, double R)
    {   x1 -= xC;
        y1 -= yC;
        x2 -= xC;
        y2 -= yC;

      double dx = x2 - x1;
      double dy = y2 - y1;

      //составляем коэффициенты квадратного уравнения на пересечение прямой и окружности.
      //если на отрезке [0..1] есть отрицательные значения, значит отрезок пересекает окружность
      double a = dx*dx + dy*dy;
      double b = 2.*(x1*dx + y1*dy);
      double c = x1*x1 + y1*y1 - R*R;

      //а теперь проверяем, есть ли на отрезке [0..1] решения
      if (-b < 0)
        return (c < 0);
      if (-b < (2.*a))
        return ((4.*a*c - b*b) < 0);

      return (a+b+c < 0);
    }

    bool intersect(double pa,double pb,double rp,double qa,double qb,double rq){
        double d;
        d = dist(pa,pb,qa,qb);

        if(d > (rq + rq)) return false;   // circles do not touch
        else if(d < abs(rp - rq)) return true;   // one circle inside the other
        else return true;
    }

    bool intersect(Circle p, Circle q) {
        return intersect(p.c.x, p.c.y, p.r, q.c.x, q.c.y, q.r);
    }

    void medium(double x1, double y1, double x2, double y2, double &mx, double &my) {
        mx = (x1 + x2) / 2.;
        my = (y1 + y2) / 2.;
    }

    void path_increment(double &x1, double &y1, double x2, double y2, double dx, double dy) {
        if (abs(x1 - x2) > dx) {
            if (x1 < x2)
                x1 = x1 + dx;
            else
                x1 = x1 - dx;
        }

        if (abs(y1 - y2) > dy) {
            if (y1 < y2)
                y1 = y1 + dy;
            else
                y1 = y1 - dy;
        }

    }


    double line_angle(Line l) {
        auto x = l.q.x - l.p.x;
        auto y = l.q.y - l.p.y;
        double angle = atan2(y, x);

        if (angle < -0.01)
            angle += 2 * M_PI;

        return angle;
        //if (angle < 0)
    }


    double two_lines_angle(Line l1, Line l2) {
        return line_angle(l2) - line_angle(l1);
    }

    Point new_point_by_vector(const Point &curr_p, double angle, double dist) {
        if (angle < 0)
            angle += 2*M_PI;

        if (angle > 2 * M_PI)
            angle -= 2*M_PI;

        Point targ_p;
        targ_p.x = curr_p.x + dist * cos(angle);
        targ_p.y = curr_p.y + dist * sin(angle);

        return targ_p;
    }



    bool check_cirlce_path_intersection_with_circle(Circle c1, Point vec, Circle c2) {
        // check next lines form intersection c1
        // from c1 center (l1)
        if (cross_line_circle(c1.c, c1.c + vec, c2))
            return true;

        // from c1 top tangent parallel to l1
        Line l(c1.c, c1.c + vec);

        double alpha = atan(l.k);
        Point topTangent, bottomTangent;

        double dtx, dty;

        // if alpha ~ 0 or endless
        if (double_equal(l.k, 0))
            dtx = 0, dty = c1.r;
        else if (double_equal(abs(l.k), std::numeric_limits<double>::max()))
            dtx = c1.r, dty = 0;
        else

        // if alpha < M_PI/2
        if (alpha < M_PI/2 || alpha > 3*M_PI/2) {
            dty = c1.r * cos(alpha);
            dtx = c1.r * cos(M_PI / 2 - alpha);
        }
        else {
            dty = c1.r * cos(M_PI / 2 - (M_PI - alpha));
            dtx = c1.r * cos(M_PI - alpha);
        }

        topTangent.set(c1.c.x + dtx, c1.c.y + dty);
        bottomTangent.set(c1.c.x - dtx, c1.c.y - dty);
        // from c1 bottom tangent parallel to l1

        return (cross_line_circle(topTangent, topTangent + vec, c2)
            || cross_line_circle(bottomTangent, bottomTangent + vec, c2));
    }

}
