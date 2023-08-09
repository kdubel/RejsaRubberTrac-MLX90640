#include "Arduino.h"
#include "spline.h"
#include <math.h>

Spline::Spline(void) {
  _prev_point = 0;
}

Spline::Spline( int16_t x[], int16_t y[], int numPoints, int degree )
{
  setPoints(x,y,numPoints);
  setDegree(degree);
  _prev_point = 0;
}

Spline::Spline( int16_t x[], int16_t y[], int16_t m[], int numPoints )
{
  setPoints(x,y,m,numPoints);
  setDegree(Hermite);
  _prev_point = 0;
}

void Spline::setPoints( int16_t x[], int16_t y[], int numPoints ) {
  _x = x;
  _y = y;
  _length = numPoints;
}

void Spline::setPoints( int16_t x[], int16_t y[], int16_t m[], int numPoints ) {
  _x = x;
  _y = y;
  _m = m;
  _length = numPoints;
}

void Spline::setDegree( int degree ){
  _degree = degree;
}

int16_t Spline::value( int16_t x )
{
  if( _x[0] > x ) { 
    return _y[0]; 
  }
  else if ( _x[_length-1] < x ) { 
    return _y[_length-1]; 
  }
  else {
    for(int i = 0; i < _length; i++ )
    {
      int index = ( i + _prev_point ) % _length;
      
      if( _x[index] == x ) {
        _prev_point = index;
        return _y[index];
      } else if( (_x[index] < x) && (x < _x[index+1]) ) {
        _prev_point = index;
        return calc( x, index );
      }
    }    
  }
  return -1; // error
}

int16_t Spline::calc( int16_t x, int i )
{
  switch( _degree ) {
    case 0:
      return _y[i];
    case 1:
      if( _x[i] == _x[i+1] ) {
        // Avoids division by 0
        return _y[i];
      } else {
        return _y[i] + (_y[i+1] - _y[i]) * ( x - _x[i]) / ( _x[i+1] - _x[i] );      
      }
    case Hermite:
      return hermite( ((x-_x[i]) / (_x[i+1]-_x[i])), _y[i], _y[i+1], _m[i], _m[i+1], _x[i], _x[i+1] );
    case Catmull:
      if( i == 0 ) {
        // x prior to spline start - first point used to determine tangent
        return _y[1];
      } else if( i == _length-2 ) {
        // x after spline end - last point used to determine tangent
        return _y[_length-2];
      } else {
        float t = (x-_x[i]) / (_x[i+1]-_x[i]);
        float m0 = (i==0 ? 0 : catmull_tangent(i) );
        float m1 = (i==_length-1 ? 0 : catmull_tangent(i+1) );
        return hermite( t, _y[i], _y[i+1], m0, m1, _x[i], _x[i+1]);        
      }
  }
  return -1; // error
}

int16_t Spline::hermite( int16_t t, int16_t p0, int16_t p1, int16_t m0, int16_t m1, int16_t x0, int16_t x1 ) {
  return (hermite_00(t)*p0) + (hermite_10(t)*(x1-x0)*m0) + (hermite_01(t)*p1) + (hermite_11(t)*(x1-x0)*m1);
}
int16_t Spline::hermite_00( int16_t t ) { return (2*pow(t,3)) - (3*pow(t,2)) + 1;}
int16_t Spline::hermite_10( int16_t t ) { return pow(t,3) - (2*pow(t,2)) + t; }
int16_t Spline::hermite_01( int16_t t ) { return (3*pow(t,2)) - (2*pow(t,3)); }
int16_t Spline::hermite_11( int16_t t ) { return pow(t,3) - pow(t,2); }

int16_t Spline::catmull_tangent( int i ) 
{ 
  if( _x[i+1] == _x[i-1] ) {
    // Avoids division by 0
    return 0;
  } else {  
    return (_y[i+1] - _y[i-1]) / (_x[i+1] - _x[i-1]);    
  } 
  return -1; // error
}
