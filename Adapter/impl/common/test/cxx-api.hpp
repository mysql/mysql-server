

/* cxx-api.hpp */

class Point {
  public:
  double x, y;
  
  Point(double initx, double inity) : x(initx) , y(inity) {};
  ~Point() {};
  
  int quadrant() {
    if(x >= 0) 
      return y < 0 ? 2 : 1;
    else
      return y < 0 ? 3 : 4;
  };  
};


class Circle {
  public:
  Point center;
  double radius;
  
  Circle(Point p, double r) : center(p), radius(r) {};
  ~Circle() {};
  double area();
};


inline double Circle::area() {
  return 3.14159264 * radius * radius;
}

