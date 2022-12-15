#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "CG_P.h"

void print_param(Param*);
void print_vertices(Vx[], int);
void print_polygon(Pg*);
void output_to_ps(Pg2DList*);
void output_to_ps1(Pg*);
void initialize_param(Param*);
void assign_flags(int, char*[], Param*);
void assign_vertices_and_polygons(FILE*, Pg**, Vx[], int*);
void assign_polygon_references(Pg*, Vx[]);
void convert_to_2D_Data(Pg*, Pg2DList**, Param*);
void transform_w_t_v(Pg2D*, Param*);
int checkInside(int, Pg2D*, Param*);
Pg* apply_back_culling(Pg*);
Pg2D* closePolygon(Pg2D*);
Pg2D* removeRedundant(Pg2D*);
Pg2D* sutherland_hodgman_clipping(Pg2D*, Param*);
Pg2D* clip_single_infinite_edge(int, Pg2D*, Param*);
Pg2D* clip_s_h(int, Pg2D*, Pg2D*, Param*);
Hc* matmul(double[4][4], Hc);
void matmul_4_4(double[4][4], double[4][4], double[4][4]);

int
main(int argc, char *argv[]) {

    Param *param = NULL;
    FILE *fp;
    Pg* polygons = NULL;
    Vx vertices[40000]; // Bunny has 69451 faces and 35947, so I assumed 40k would be enough
    int i, num_of_vertices;
    Hc *temp_homogeneous_coord;

    param = malloc(sizeof(Param));

    // Initialize (flags to default values):
    initialize_param(param);
    
    // Flag checking : -a, -b, -c, -d, -f, -m, -n, -r, -s, -j, -k, -o, -p
    assign_flags(argc, argv, param);

    // Debug 
    //print_param(param);
    // File reading:
    fp = fopen(param->f, "r");
    if(fp == NULL) {
        fprintf(stderr, "Could not open file: %s\n", param->f);
        exit(1);
    }

    assign_vertices_and_polygons(fp, &polygons, vertices, &num_of_vertices);
    // Debug
    //print_param(param);
    //print_vertices(vertices, num_of_vertices);    
    //print_polygon(polygons);
    
    /*
    Matrices to be used:
    M_PER : Perspective projection
    M_PAR : Orthogonal projection
    T : Translate VRP to origin
    R : Rotate VPN to z, VUP to y
    T_PRP : Translate PRP in VRC
    Shear : Shear 
    T_par : Translate the center of the volumn to origin
    S_par : Scale back
    S_per : Scale Perspective
    */

    double T[4][4] = {  
                {1,0,0,-param->X},
                {0,1,0,-param->Y},
                {0,0,1,-param->Z},
                {0,0,0,1}
            };
    Vx VPN = { .x = param->q, .y = param->r, .z = param->w };

    double magnitude_VPN = sqrt(VPN.x * VPN.x + VPN.y * VPN.y + VPN.z * VPN.z);

    if(magnitude_VPN == 0.0) {
        printf("Division by error in magnitude VPN\n");
        exit(1);
    }

    Vx Rz = { .x = VPN.x / magnitude_VPN, .y = VPN.y / magnitude_VPN, .z = VPN.z / magnitude_VPN};

    Vx VUP = { .x = param->Q, .y = param->R, .z = param->W};

    Vx VUPxRz = { .x = VUP.y * Rz.z - VUP.z * Rz.y, .y = VUP.z * Rz.x - VUP.x * Rz.z, .z = VUP.x * Rz.y - VUP.y * Rz.x };

    double magnitude_VUPxRz = sqrt(VUPxRz.x * VUPxRz.x + VUPxRz.y * VUPxRz.y + VUPxRz.z * VUPxRz.z);

    if(magnitude_VUPxRz == 0.0) {
        printf("Division by zero error in magnitude VUPxRz\n");
        exit(1);
    }

    Vx Rx = { .x = VUPxRz.x / magnitude_VUPxRz, .y = VUPxRz.y / magnitude_VUPxRz, .z = VUPxRz.z / magnitude_VUPxRz };

    Vx Ry = { .x = Rz.y * Rx.z - Rx.y * Rz.z, .y = Rz.z * Rx.x - Rz.x * Rx.z, .z = Rz.x * Rx.y - Rz.y * Rx.x };

    
    double R[4][4] = {
                {Rx.x, Rx.y, Rx.z, 0},
                {Ry.x, Ry.y, Ry.z, 0},
                {Rz.x, Rz.y, Rz.z, 0},
                {0, 0, 0, 1}
            };

    Vx PRP = { .x = param->x, .y = param->y, .z = param->z };

    double T_PRP[4][4] = {
                {1, 0, 0, -PRP.x},
                {0, 1, 0, -PRP.y},
                {0, 0, 1, -PRP.z},
                {0, 0, 0, 1}
            };

    double Shear[4][4] = {
                {1, 0, ((1.0/2.0) * (param->U + param->u) - PRP.x)/PRP.z, 0},
                {0, 1, ((1.0/2.0) * (param->V + param->v) - PRP.y)/PRP.z, 0},
                {0, 0, 1, 0},
                {0, 0, 0, 1}
            };

    double T_par[4][4] = {
                {1, 0, 0, -(param->U + param->u)/2},
                {0, 1, 0, -(param->V + param->v)/2},
                {0, 0, 1, -param->F},
                {0, 0, 0, 1}
            };
    double S_par[4][4] = {
                {2/(param->U - param->u), 0, 0, 0},
                {0, 2/(param->V - param->v), 0, 0},
                {0, 0, 1/(param->F - param->B), 0},
                {0, 0, 0, 1}
            };

    double S_per[4][4] = {
                {(2 * PRP.z) / ((param->U - param->u) * (PRP.z - param->B)), 0, 0, 0},
                {0, (2 * PRP.z) / ((param->V - param->v) * (PRP.z - param->B)), 0, 0},
                {0, 0, 1 / (PRP.z - param->B), 0},
                {0, 0, 0, 1}
            };

    if(param->use_parallel) { // Parallel projection

        double N_par[4][4] = {0.0};
        matmul_4_4(R, T, N_par);
        matmul_4_4(Shear, N_par, N_par);
        matmul_4_4(T_par, N_par, N_par);
        matmul_4_4(S_par, N_par, N_par);

        for(i = 1; i < num_of_vertices; i++) {
            Hc new = { .x = vertices[i].x, .y = vertices[i].y, .z = vertices[i].z, .w = 1};
            temp_homogeneous_coord = matmul(N_par, new);
            vertices[i].x = temp_homogeneous_coord->x / temp_homogeneous_coord->w;
            vertices[i].y = temp_homogeneous_coord->y / temp_homogeneous_coord->w;
            vertices[i].z = temp_homogeneous_coord->z / temp_homogeneous_coord->w;
        }
        // DEBUG
        //double ans[4][4] = { 0 };
        //matmul_4_4(R, T, ans);
        //matmul_4_4(Shear, ans, ans);
        //matmul_4_4(T_par, ans, ans);
        //matmul_4_4(S_par, ans, ans);
        //for(i = 0; i < 4; i++) {
        //    printf("[ %lf\t%lf\t%lf\t%lf ]\n", ans[i][0], ans[i][1], ans[i][2], ans[i][3]);
        //} 
    }

    else { // Perspective projection
        
        double N_per[4][4] = {0.0};
        matmul_4_4(R, T, N_per);
        matmul_4_4(T_PRP, N_per, N_per);
        matmul_4_4(Shear, N_per, N_per);
        matmul_4_4(S_per, N_per, N_per);

        for(i = 1; i < num_of_vertices; i++) {
            Hc new = { .x = vertices[i].x, .y = vertices[i].y, .z = vertices[i].z, .w = 1};
            temp_homogeneous_coord = matmul(N_per, new);
            vertices[i].x = temp_homogeneous_coord->x / temp_homogeneous_coord->w;
            vertices[i].y = temp_homogeneous_coord->y / temp_homogeneous_coord->w;
            vertices[i].z = temp_homogeneous_coord->z / temp_homogeneous_coord->w;
        }
        //double ans[4][4] = { 0 };
        //matmul_4_4(R, T, ans);
        //matmul_4_4(T_PRP, ans, ans);
        //matmul_4_4(Shear, ans, ans);
        //matmul_4_4(S_per, ans, ans);
        //for(i = 0; i < 4; i++) {
        //    printf("[ %lf\t%lf\t%lf\t%lf ]\n", ans[i][0], ans[i][1], ans[i][2], ans[i][3]);
        //}
    }

    // Convert to 2D:
    assign_polygon_references(polygons, vertices);
   
    if(param->back_cull) {
        // Back culling:
        polygons = apply_back_culling(polygons);
    }

    Pg2DList *polygons2D = NULL;
    Pg2DList *polygons2D_traverser = NULL;

    convert_to_2D_Data(polygons, &polygons2D, param);
    // DEBUG
    //while(polygons2D != NULL) {
    //    while(polygons2D->curr != NULL) {
    //        printf("x: %lf, y: %lf\n", polygons2D->curr->x, polygons2D->curr->y);
    //        polygons2D->curr = polygons2D->curr->next;
    //    }
    //    polygons2D = polygons2D->next;
    //}
    // Clipping in 2D
    polygons2D_traverser = polygons2D;
    while(polygons2D_traverser != NULL) { // For each polygon
        polygons2D_traverser->curr = sutherland_hodgman_clipping(polygons2D_traverser->curr, param);
        transform_w_t_v(polygons2D_traverser->curr, param);
        polygons2D_traverser = polygons2D_traverser->next; 
    }


    // Output:
    output_to_ps(polygons2D);

    free(param);
    exit(0);
}

void
matmul_4_4(double A[4][4], double B[4][4], double ans[4][4]) {

    int i, j, k;
    double temp;
    double temp_arr[4][4];

    for(i = 0; i < 4; i++) {
        for(j = 0; j < 4; j++) {
            temp_arr[i][j] = B[i][j];
        }
    }

    for(i = 0; i < 4; i++) {
        for(j = 0; j < 4; j++) {
            temp = 0.0;
            for(k = 0; k < 4; k++) {
                temp += A[i][k] * temp_arr[k][j];
            }
            ans[i][j] = temp;
        }
    }
}

Pg*
apply_back_culling(Pg *polygons) {

    Pg *new_polygon, *traverser;
    new_polygon = NULL;

    while(polygons != NULL) { // For each polygon
        Vx vec1 = {polygons->tri->vertices[1].x - polygons->tri->vertices[0].x,
                   polygons->tri->vertices[1].y - polygons->tri->vertices[0].y, 
                   polygons->tri->vertices[1].z - polygons->tri->vertices[0].z};

        Vx vec2 = {polygons->tri->vertices[2].x - polygons->tri->vertices[0].x,
                   polygons->tri->vertices[2].y - polygons->tri->vertices[0].y, 
                   polygons->tri->vertices[2].z - polygons->tri->vertices[0].z};

        Vx new = {(vec1.y * vec2.z) - (vec2.y * vec1.z),
                  (vec2.x * vec1.z) - (vec1.x * vec2.z),
                  (vec1.x * vec2.y) - (vec2.x * vec1.y)};

        if(new.z >= 0) { // add to the new polygon list
            if(new_polygon == NULL) {
                new_polygon = malloc(sizeof(Pg));
                new_polygon->tri = polygons->tri;
                new_polygon->next = NULL;
                traverser = new_polygon;
            }
            else {
                traverser->next = malloc(sizeof(Pg));
                traverser->next->tri = polygons->tri;
                traverser->next->next = NULL;
                traverser = traverser->next;
            }
        }
        polygons = polygons->next;
    }
    return new_polygon;
}

Pg2D*
sutherland_hodgman_clipping(Pg2D* polygon, Param* param) {

    int i;
    Pg2D* newpolygon = polygon;
    for(i = 0; i < 4; i++) {
        newpolygon = clip_single_infinite_edge(i, newpolygon, param);
        newpolygon = removeRedundant(newpolygon);
        newpolygon = closePolygon(newpolygon);
    }
    return newpolygon;
}

Pg2D*
clip_single_infinite_edge(int i, Pg2D* polygon, Param* param) {

    Pg2D* toadd = NULL;
    Pg2D* traverser = NULL;
    Pg2D* new_polygon = NULL;
    Pg2D* next_polygon = NULL;

    while(polygon != NULL) {
        
        // Allocate memory for vertex to be added
        toadd = malloc(sizeof(Pg2D));
        toadd->next = NULL;

        // Check the four cases:
        if(checkInside(i, polygon, param)) { // if inside
            toadd->x = polygon->x;
            toadd->y = polygon->y;
            if(new_polygon == NULL) { // new polygon in not initialized
                new_polygon = toadd;
                traverser = new_polygon;
            }
            else {
                traverser->next = toadd;
                traverser = traverser->next;
            }
            next_polygon = polygon->next; // Check next point
            if(next_polygon == NULL) break;
            if(!checkInside(i, next_polygon, param)) { // If not inside, clip
                toadd = clip_s_h(i, polygon, next_polygon, param);
                traverser->next = toadd;
                traverser = traverser->next;
            }
        }
        else if(!checkInside(i, polygon, param)) { // If not inside, don't add
            next_polygon = polygon->next;
            if(next_polygon == NULL) break;
            if(checkInside(i, next_polygon, param)) { // check next point
                toadd = clip_s_h(i, polygon, next_polygon, param);
                if(new_polygon == NULL) { // new polygon is not initialized
                    new_polygon = toadd;
                    traverser = new_polygon;
                }
                else {
                    traverser->next = toadd;
                    traverser = toadd;
                }
            }
        }
        polygon = polygon->next;
    }
    return new_polygon;
}

int
checkInside(int i, Pg2D* polygon, Param* param) {

    int isInside = 1;

    if(param->use_parallel) {
        if(i == 0) { // Right clip boundary
            if(polygon->x > 1.0) isInside = 0;
        }
        else if(i == 1) { // top clip boundary
            if(polygon->y > 1.0) isInside = 0;
        }
        else if(i == 2) { // left clip boundary
            if(polygon->x < -1.0) isInside = 0;
        }
        else if(i == 3) { // bottom clip boundary
            if(polygon->y < -1.0) isInside = 0;
        }
    }
    else {
        double d = fabs(param->z / (param->B - param->z));
        if(i == 0) { // Right clip boundary
            if(polygon->x > d) isInside = 0;
        }
        else if(i == 1) { // top clip boundary
            if(polygon->y > d) isInside = 0;
        }
        else if(i == 2) { // left clip boundary
            if(polygon->x < -d) isInside = 0;
        }
        else if(i == 3) { // bottom clip boundary
            if(polygon->y < -d) isInside = 0;
        }
    }

    return isInside;
}

Pg2D*
clip_s_h(int i, Pg2D* polygon, Pg2D* next_polygon, Param* param) {

    Pg2D* temp = malloc(sizeof(Pg2D));

    temp->next = NULL;

    if(param->use_parallel) {
        if(i == 0) { // right clip boundary
            temp->y = (polygon->x - 1.0) * (next_polygon->y - polygon->y) / (polygon->x - next_polygon->x) + polygon->y;
            temp->x = 1.0;
        }
        else if(i == 1) { // top clip boundary
            temp->x = (1.0 - polygon->y) * (polygon->x - next_polygon->x) / (polygon->y - next_polygon->y) + polygon->x;
            temp->y = 1.0;
        }
        else if(i == 2) { // left clip boundary
            temp->y = (-1.0 - polygon->x) * (next_polygon->y - polygon->y) / (next_polygon->x - polygon->x) + polygon->y;
            temp->x = -1.0;
        }
        else if(i == 3) { // bottom clip boundary
            temp->x = (polygon->y + 1.0) * (next_polygon->x - polygon->x) / (polygon->y - next_polygon->y) + polygon->x;
            temp->y = -1.0;
        }
    }

    else {
        double d = fabs(param->z / (param->B - param->z));
        if(i == 0) { // right clip boundary
            temp->y = (polygon->x - d) * (next_polygon->y - polygon->y) / (polygon->x - next_polygon->x) + polygon->y;
            temp->x = d;
        }
        else if(i == 1) { // top clip boundary
            temp->x = (d - polygon->y) * (polygon->x - next_polygon->x) / (polygon->y - next_polygon->y) + polygon->x;
            temp->y = d;
        }
        else if(i == 2) { // left clip boundary
            temp->y = (-d - polygon->x) * (next_polygon->y - polygon->y) / (next_polygon->x - polygon->x) + polygon->y;
            temp->x = -d;
        }
        else if(i == 3) { // bottom clip boundary
            temp->x = (polygon->y + d) * (next_polygon->x - polygon->x) / (polygon->y - next_polygon->y) + polygon->x;
            temp->y = -d;
        }
    }

    return temp;
}
 
Pg2D*
closePolygon(Pg2D* polygon) {

    Pg2D* new_polygon = polygon;
    Pg2D* last = NULL;
    Pg2D* temp = NULL;

    if(polygon == NULL) return NULL;
    while(polygon != NULL) {
        last = polygon;
        polygon = polygon->next;
    }
    if((last->x != new_polygon->x) || (last->y != new_polygon->y)) {
        temp = malloc(sizeof(Pg2D));
        temp->next = NULL;
        temp->x = new_polygon->x;
        temp->y = new_polygon->y;
        last->next = temp;
    }
    return new_polygon;
}

void
transform_w_t_v(Pg2D* polygon, Param* param) {

    double sX, sY; // Scale factors
    double new_x, new_y;
    while(polygon != NULL) {
        if(param->use_parallel) {
            new_x = polygon->x + 1.0;
            new_y = polygon->y + 1.0;
            sX = (double) (param->o - param->j) / 2.0; 
            sY = (double) (param->p - param->k) / 2.0;
            new_x *= sX;
            new_y *= sY;
            new_x += param->j;
            new_y += param->k;
        }
        else {
            double d = fabs(param->z / (param->B - param->z));
            new_x = polygon->x + d;
            new_y = polygon->y + d;
            sX = (double) (param->o - param->j) / (double) (2.0 * d);
            sY = (double) (param->p - param->k) / (double) (2.0 * d);
            new_x *= sX;
            new_y *= sY;
            new_x += param->j;
            new_y += param->k;
        }

        polygon->x = new_x;
        polygon->y = new_y;

        polygon = polygon->next;
    }
}

Pg2D*
removeRedundant(Pg2D* polygon) {

    Pg2D* new_polygon = polygon;
    Pg2D* traverser;

    if(polygon == NULL) return NULL;
    traverser = polygon;
    traverser = traverser->next;
    if(traverser == NULL) return new_polygon; // If we only have one point
    while(traverser->next != NULL) {
        if((traverser->x == traverser->next->x) && (traverser->y == traverser->next->y)) { // redundant
            if(traverser->next->next != NULL) {
                traverser->next = traverser->next->next;
                traverser = traverser->next;
            }
            else {
                traverser->next = NULL;
                return new_polygon;
            }
        }
        else {
            traverser = traverser->next;
        }
    }
    return new_polygon;
}

void
convert_to_2D_Data(Pg* polygons, Pg2DList** polygons2D, Param* param) {

    Pg2D *temp_pg2d, *temp_temp_pg2d, *pg2d_traverser;
    Pg2DList *pg2dlist_traverser;
    Hc *temp_homogeneous_coord;
    int i;
    Vx PRP = { .x = param->x, .y = param->y, .z = param->z };
    double d = PRP.z / (param->B - PRP.z);
    double M_per[4][4] = {
                    {1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 1, 0},
                    {0, 0, 1/d, 0}
                };

    double M_par[4][4] = {
                    {1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 0, 0},
                    {0, 0, 0, 1}
                };

    while(polygons != NULL) {
        temp_pg2d = NULL;
        pg2d_traverser = NULL;
        for(i = 0; i < 4; i++) {
            temp_temp_pg2d = malloc(sizeof(Pg2D));

            if(i < 3) {
                Hc new = {polygons->tri->vertices[i].x, polygons->tri->vertices[i].y, polygons->tri->vertices[i].z, 1};
                if(param->use_parallel) {
                    temp_homogeneous_coord = matmul(M_par, new);
                }
                else {
                    temp_homogeneous_coord = matmul(M_per, new);
                }
                polygons->tri->vertices[i].x = temp_homogeneous_coord->x / temp_homogeneous_coord->w;
                polygons->tri->vertices[i].y = temp_homogeneous_coord->y / temp_homogeneous_coord->w;
                polygons->tri->vertices[i].z = temp_homogeneous_coord->z / temp_homogeneous_coord->w;
            }

            if(i == 3) {
                temp_temp_pg2d->x = polygons->tri->vertices[0].x;
                temp_temp_pg2d->y = polygons->tri->vertices[0].y;
                temp_temp_pg2d->next = NULL;
            }
            else {
                temp_temp_pg2d->x = polygons->tri->vertices[i].x;
                temp_temp_pg2d->y = polygons->tri->vertices[i].y;
                temp_temp_pg2d->next = NULL;
            }
            if(temp_pg2d == NULL) { // If temp_pg2 has no pointer
                temp_pg2d = temp_temp_pg2d;
                pg2d_traverser = temp_pg2d;
            }
            else {
                pg2d_traverser->next = temp_temp_pg2d;
                pg2d_traverser = pg2d_traverser->next;
            }
        
        }
        if(*polygons2D == NULL) {
            *polygons2D = malloc(sizeof(Pg2DList));
            (*polygons2D)->curr = temp_pg2d;
            (*polygons2D)->next = NULL;
            pg2dlist_traverser = *polygons2D;
        }
        else {
            pg2dlist_traverser->next = malloc(sizeof(Pg2DList));
            pg2dlist_traverser->next->curr = temp_pg2d;
            pg2dlist_traverser->next->next = NULL;
            pg2dlist_traverser = pg2dlist_traverser->next;
        }

        polygons = polygons->next;
    }
}
            


Hc*
matmul(double mat[4][4], Hc coord) {

    Hc* temp_coord = malloc(sizeof(Hc));
    double temp;
    int i, j;
    double temp_mat[4] = { coord.x, coord.y, coord.z, coord.w };

    for(i = 0; i < 4; i++) {
        temp = 0;
        for(j = 0; j < 4; j++) {
            temp += mat[i][j] * temp_mat[j];
        }
        if(i == 0) temp_coord->x = temp;
        else if(i == 1) temp_coord->y = temp;
        else if(i == 2) temp_coord->z = temp;
        else if(i == 3) temp_coord->w = temp;
    }

    if(temp_coord->w == 0) {
        printf("w in homogeneous coordinate is 0\n");
        exit(1);
    }
    
    return temp_coord;
}

void assign_polygon_references(Pg *polygons, Vx vertices[]) {

    int i;

    while(polygons != NULL) {
        for(i = 0; i < 3; i++) {
            polygons->tri->vertices[i] = vertices[polygons->tri->references[i]];
        }
        polygons = polygons->next;
    }
}

void
assign_vertices_and_polygons(FILE* fp, Pg **polygons, Vx vertices[], int* num_of_vertices) {

    char temp[BUFSIZE], *line, *tok;
    int i;
    size_t size;
    Pg* traverser, *temp_polygon;
    Tr* temp_triangle;

    line = NULL;

    i = 0; // counter for vertices
    // Skip zero index
    vertices[i].x = 0.0;
    vertices[i].y = 0.0;
    vertices[i].z = 0.0;
    i++;
    while(getline(&line, &size, fp) != -1) {
        if(*line == 'v') { // First character is v for vertex
            strncpy(temp, line, BUFSIZE);
            tok = strtok(temp, " \t\n"); // the first token would be "v"
            tok = strtok(NULL, " \t\n"); // x
            vertices[i].x = atof(tok);
            tok = strtok(NULL, " \t\n"); // y
            vertices[i].y = atof(tok);
            tok = strtok(NULL, " \t\n"); // z
            vertices[i].z = atof(tok);
            i++;
        }
        else if(*line == 'f') { // First character is f for face
            strncpy(temp, line, BUFSIZE);
            tok = strtok(temp, " \t\n"); // the first token would be "f"
            temp_polygon = malloc(sizeof(Pg));
            temp_polygon->next = NULL;
            temp_triangle = malloc(sizeof(Tr));
            tok = strtok(NULL, " \t\n");
            temp_triangle->references[0] = atoi(tok);
            tok = strtok(NULL, " \t\n");
            temp_triangle->references[1] = atoi(tok);
            tok = strtok(NULL, " \t\n");
            temp_triangle->references[2] = atoi(tok);
            temp_polygon->tri = temp_triangle;
            // If polygons is NULL
            if(*polygons == NULL) {
                *polygons = temp_polygon;
            }
            else {
                traverser = *polygons;
                while(traverser->next != NULL) traverser = traverser->next;
                traverser->next = temp_polygon;
            }
        } 
    }
    *num_of_vertices = i;
}

void
output_to_ps(Pg2DList* polygons_list) {

    int i;

    printf("%%%%BeginSetup\n\t<< /PageSize [501 501] >> setpagedevice\n%%%%EndSetup\n\n");
    printf("%%%%%%BEGIN\n");
    while(polygons_list != NULL) { // For each polygon
        if(polygons_list->curr == NULL) {
            polygons_list = polygons_list->next;
            continue;
        }
        i = 0;
        while(polygons_list->curr != NULL) { // for each point
            if(i == 0) {
                printf("%d %d moveto\n", (int) (polygons_list->curr->x), (int) (polygons_list->curr->y));
            }
            else {
                printf("%d %d lineto\n", (int) (polygons_list->curr->x), (int) (polygons_list->curr->y));
            }
            polygons_list->curr = polygons_list->curr->next;
            i++;
        }
        printf("stroke\n");
        polygons_list = polygons_list->next;
    }
    printf("%%%%%%END\n");
}

void
output_to_ps1(Pg* polygons) {

    printf("%%%%%%BEGIN\n");
    while(polygons != NULL) { // For each polygon
        printf("%d %d moveto\n", (int) (polygons->tri->vertices[0].x), (int) (polygons->tri->vertices[0].y));
        printf("%d %d lineto\n", (int) (polygons->tri->vertices[1].x), (int) (polygons->tri->vertices[1].y));
        printf("%d %d lineto\n", (int) (polygons->tri->vertices[2].x), (int) (polygons->tri->vertices[2].y));
        printf("%d %d lineto\n", (int) (polygons->tri->vertices[0].x), (int) (polygons->tri->vertices[0].y));
        printf("stroke\n\n");
        polygons = polygons->next;
    }
    printf("%%%%%%END\n");
}

            
void
print_polygon(Pg *polygon) {

    int i;

    while(polygon != NULL) {
        printf("Vertices:\n");
        for(i = 0; i < 3; i++) {
            printf("%d: x: %lf\ty: %lf\tz: %lf\n", i+1, polygon->tri->vertices[i].x, polygon->tri->vertices[i].y, polygon->tri->vertices[i].z);
        }
        polygon = polygon->next;
    }
}

void
print_vertices(Vx vertices[], int num_of_vertices) {
    
    int i = 1;
    while(i < num_of_vertices) {
        printf("Vertex: %d\n-x: %lf\n-y: %lf\n-z: %lf\n", i, vertices[i].x, vertices[i].y, vertices[i].z);
        i++;
    }
}
   
// Function to take the flag arguments and assign it to the members of Factors struct
void
assign_flags(int argc, char *argv[], Param *param) {

    int i;

    i = 1; // Skip argv[0] which is the command.

    while(i < argc) {
            if(strcmp(argv[i], "-f") == 0) {
                strncpy(param->f, argv[++i], BUFSIZE-1);
                (param->f)[BUFSIZE] = '\0';
            }
            else if(strcmp(argv[i], "-j") == 0) {
                param->j = atoi(argv[++i]);
            }
            else if(strcmp(argv[i], "-k") == 0) {
                param->k = atoi(argv[++i]);
            }
            else if(strcmp(argv[i], "-o") == 0) {
                param->o = atoi(argv[++i]);
            }
            else if(strcmp(argv[i], "-p") == 0) {
                param->p = atoi(argv[++i]);
            }
            else if(strcmp(argv[i], "-x") == 0) {
                param->x = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-y") == 0) {
                param->y = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-z") == 0) {
                param->z = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-X") == 0) {
                param->X = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-Y") == 0) {
                param->Y = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-Z") == 0) {
                param->Z = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-q") == 0) {
                param->q = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-r") == 0) {
                param->r = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-w") == 0) {
                param->w = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-Q") == 0) {
                param->Q = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-R") == 0) {
                param->R = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-W") == 0) {
                param->W = atoi(argv[++i]);
            }
            else if(strcmp(argv[i], "-u") == 0) {
                param->u = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-v") == 0) {
                param->v = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-U") == 0) {
                param->U = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-V") == 0) {
                param->V = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-P") == 0) {
                param->use_parallel = 1;
            }
            else if(strcmp(argv[i], "-b") == 0) {
                param->back_cull = 1;
            }
            else if(strcmp(argv[i], "-F") == 0) {
                param->F = atof(argv[++i]);
            }
            else if(strcmp(argv[i], "-B") == 0) {
                param->B = atof(argv[++i]);
            }
            i++;
        }
}

// Function that initializes a Factors struct to default values
void
initialize_param(Param *param) {
    
    strncpy(param->f, "bound-lo-sphere.smf", BUFSIZE-1); // input filename
    (param->f)[BUFSIZE] = '\0';
    param->j = 0; // lower bound in x dim of viewport in screen coord
    param->k = 0; // lower bound in y dim of viewport in screen coord
    param->o = 500; // upper bound in x dim of viewport in screen coord
    param->p = 500; // upper bound in y dim of viewport in screen coord
    param->x = 0.0; // x of PRP in VRC coord
    param->y = 0.0; // y of PRP in VRC coord
    param->z = 1.0; // z of PRP in VRC coord
    param->X = 0.0; // x of VRP in world coord
    param->Y = 0.0; // y of VRP in world coord
    param->Z = 0.0; // z of VRP in world coord
    param->q = 0.0; // x of VPN in world coord
    param->r = 0.0; // y of VPN in world coord
    param->w = -1.0; // z of VPN in world coord
    param->Q = 0.0; // x of VUP in world coord
    param->R = 1.0; // y of VUP in world coord
    param->W = 0.0; // z of VUP in world coord
    param->u = -0.7; // u min of VRC window in VRC coord
    param->v = -0.7; // v min of VRC window in VRC coord
    param->U = 0.7; // u max of VRC window in VRC coord
    param->V = 0.7; // v max of VRC window in VRC coord
    param->F = 0.6; // front plane
    param->B = -0.6; // Back plane
    param->use_parallel = 0; // boolean to use parallel if 1
    param->back_cull = 0;

}

void
print_param(Param *param) {

    printf("Parameter settings:\n-f: %s\n-j: %d\n-k: %d\n-o: %d\n-p: %d\n-x: %lf\n-y: %lf\n-z: %lf\n-X: %lf\n-Y: %lf\n-Z: %lf\n-q: %lf\n-r: %lf\n-w: %lf\n-Q: %lf\n-R: %lf\n-W: %lf\n-u: %lf\n-v: %lf\n-U: %lf\n-V: %lf\n-P: %d\n-F: %lf\n-B: %lf\n", param->f, param->j, param->k, param->o, param->p, param->x, param->y, param->z, param->X, param->Y, param->Z, param->q, param->r, param->w, param->Q, param->R, param->W, param->u, param->v, param->U, param->V, param->use_parallel, param->F, param->B);

} 
