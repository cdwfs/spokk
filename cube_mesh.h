/* Primitive type is TRIANGLE_LIST */
static int cube_vertex_count = 24;
static int cube_index_count = 36;
static struct {
    float position[3];
    float normal[3];
    float texcoord[2];
} cube_vertices[24] = {
	0.500000000f,-0.500000000f,0.500000000f, 1.000000000f,0.000000000f,0.000000000f, 0.000000000f,1.000000000f,
	0.500000000f,-0.500000000f,-0.500000000f, 1.000000000f,0.000000000f,0.000000000f, 1.000000000f,1.000000000f,
	0.500000000f,0.500000000f,0.500000000f, 1.000000000f,0.000000000f,0.000000000f, 0.000000000f,0.000000000f,
	0.500000000f,0.500000000f,-0.500000000f, 1.000000000f,0.000000000f,0.000000000f, 1.000000000f,0.000000000f,
	-0.500000000f,-0.500000000f,-0.500000000f, -1.000000000f,0.000000000f,0.000000000f, 0.000000000f,1.000000000f,
	-0.500000000f,-0.500000000f,0.500000000f, -1.000000000f,0.000000000f,0.000000000f, 1.000000000f,1.000000000f,
	-0.500000000f,0.500000000f,-0.500000000f, -1.000000000f,0.000000000f,0.000000000f, 0.000000000f,0.000000000f,
	-0.500000000f,0.500000000f,0.500000000f, -1.000000000f,0.000000000f,0.000000000f, 1.000000000f,0.000000000f,
	-0.500000000f,0.500000000f,0.500000000f, 0.000000000f,1.000000000f,0.000000000f, 0.000000000f,1.000000000f,
	0.500000000f,0.500000000f,0.500000000f, 0.000000000f,1.000000000f,0.000000000f, 1.000000000f,1.000000000f,
	-0.500000000f,0.500000000f,-0.500000000f, 0.000000000f,1.000000000f,0.000000000f, 0.000000000f,0.000000000f,
	0.500000000f,0.500000000f,-0.500000000f, 0.000000000f,1.000000000f,0.000000000f, 1.000000000f,0.000000000f,
	-0.500000000f,-0.500000000f,-0.500000000f, 0.000000000f,-1.000000000f,0.000000000f, 0.000000000f,1.000000000f,
	0.500000000f,-0.500000000f,-0.500000000f, 0.000000000f,-1.000000000f,0.000000000f, 1.000000000f,1.000000000f,
	-0.500000000f,-0.500000000f,0.500000000f, 0.000000000f,-1.000000000f,0.000000000f, 0.000000000f,0.000000000f,
	0.500000000f,-0.500000000f,0.500000000f, 0.000000000f,-1.000000000f,0.000000000f, 1.000000000f,0.000000000f,
	-0.500000000f,-0.500000000f,0.500000000f, 0.000000000f,0.000000000f,1.000000000f, 0.000000000f,1.000000000f,
	0.500000000f,-0.500000000f,0.500000000f, 0.000000000f,0.000000000f,1.000000000f, 1.000000000f,1.000000000f,
	-0.500000000f,0.500000000f,0.500000000f, 0.000000000f,0.000000000f,1.000000000f, 0.000000000f,0.000000000f,
	0.500000000f,0.500000000f,0.500000000f, 0.000000000f,0.000000000f,1.000000000f, 1.000000000f,0.000000000f,
	0.500000000f,-0.500000000f,-0.500000000f, 0.000000000f,0.000000000f,-1.000000000f, 0.000000000f,1.000000000f,
	-0.500000000f,-0.500000000f,-0.500000000f, 0.000000000f,0.000000000f,-1.000000000f, 1.000000000f,1.000000000f,
	0.500000000f,0.500000000f,-0.500000000f, 0.000000000f,0.000000000f,-1.000000000f, 0.000000000f,0.000000000f,
	-0.500000000f,0.500000000f,-0.500000000f, 0.000000000f,0.000000000f,-1.000000000f, 1.000000000f,0.000000000f,
};
static uint32_t cube__indices[] = {
	         0,         1,         2,         2,         1,         3,
	         4,         5,         6,         6,         5,         7,
	         8,         9,        10,        10,         9,        11,
	        12,        13,        14,        14,        13,        15,
	        16,        17,        18,        18,        17,        19,
	        20,        21,        22,        22,        21,        23,
};
