#define BUFSIZE 1024
#define ever ;;

typedef struct Parameters Param;
typedef struct Vertex Vx;
typedef struct Triangle Tr;
typedef struct Polygons Pg;
typedef struct Homogeneous_coord Hc;
typedef struct Polygon2D Pg2D;
typedef struct Polygon2DList Pg2DList;


struct Polygon2D {

    double x;
    double y;
    Pg2D* next;
};

struct Polygon2DList {
    
    Pg2D* curr;
    Pg2DList* next;

};

struct Homogeneous_coord {

    double x;
    double y;
    double z;
    double w;

};

struct Vertex {

    double x;
    double y;
    double z;

};

struct Triangle {

    int references[3];
    Vx vertices[3];

};

struct Polygons {

    Tr* tri;
    Pg* next;

};

struct Parameters {

    char f[BUFSIZE]; // input filename
    int j; // lower bound in x dim of viewport in screen coord
    int k; // lower bound in y dim of viewport in screen coord
    int o; // upper bound in x dim of viewport in screen coord
    int p; // upper bound in y dim of viewport in screen coord
    double x; // x of PRP in VRC coord
    double y; // y of PRP in VRC coord
    double z; // z of PRP in VRC coord
    double X; // x of VRP in world coord
    double Y; // y of VRP in world coord
    double Z; // z of VRP in world coord
    double q; // x of VPN in world coord
    double r; // y of VPN in world coord
    double w; // z of VPN in world coord
    double Q; // x of VUP in world coord
    double R; // y of VUP in world coord
    double W; // z of VUP in world coord
    double u; // u min of VRC window in VRC coord
    double v; // v min of VRC window in VRC coord
    double U; // u max of VRC window in VRC coord
    double V; // v max of VRC window in VRC coord
    double F; // Front plane
    double B; // Back plane
    int use_parallel; // boolean to use parallel if 1
    int back_cull; // boolean for backculling
}; 
