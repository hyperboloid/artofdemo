#ifndef __VECTOR_H_
#define __VECTOR_H_

#include <cmath>

class VECTOR
{
	float v[3];

public:

	      float &operator[](const int i)       { return v[i]; }
	const float &operator[](const int i) const { return v[i]; }


	VECTOR operator+(const VECTOR &a) const
        {
               VECTOR r;
               r[0] = v[0] + a[0];
               r[1] = v[1] + a[1];
               r[2] = v[2] + a[2];
               return r;
        }

	VECTOR operator-(const VECTOR &a) const
        {
               VECTOR r;
               r[0] = v[0] - a[0];
               r[1] = v[1] - a[1];
               r[2] = v[2] - a[2];
               return r;
        }

	VECTOR &operator=(const VECTOR &a)
	{
		v[0] = a[0];
		v[1] = a[1];
		v[2] = a[2];
		return *this;
	}

        VECTOR() {}
        VECTOR(const float X, const float Y, const float Z) { v[0]=X; v[1]=Y; v[2]=Z; }
        ~VECTOR() {}
};

inline VECTOR normalize(const VECTOR &a)
{
   float  id = 1/sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
   return VECTOR( a[0]*id, a[1]*id, a[2]*id );
}


#endif
