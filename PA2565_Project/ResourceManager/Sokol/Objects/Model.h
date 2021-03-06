#ifndef _RM_MODEL_
#define _RM_MODEL_

#include <vector>
#include <mutex>

#include "../../Defines.h"
#include "Transform.hpp"

class Resource;
class MeshResource;
class TextureResource;

class Model {
private:
	MeshResource* m_mesh;
	TextureResource* m_texture;
	Transform m_transform;
	unsigned int m_vertexCount;
	unsigned int m_indexCount;

	std::mutex tempMutex;

public:
	Model();
	Model(MeshResource * mesh, TextureResource * tex);
	~Model();

	void draw(sg_draw_state& drawState, vs_params_t& vsParams);

	void setMesh(MeshResource* mesh);
	// Used for async callback
	void setMeshNoDeref(Resource* mesh);
	// Used for async callback
	void setMeshCallback(Resource* mesh);

	void setTexture(TextureResource* tex);
	// Used for async callback
	void setTexNoDeref(Resource* tex);
	// Used for async callback
	void setTexCallback(Resource* tex);

	Transform& getTransform();

};

#endif //_RM_MODEL_