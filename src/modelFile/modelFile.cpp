/** Copyright (C) 2013 David Braam - Released under terms of the AGPLv3 License */
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "modelFile.h"
#include "../utils/logoutput.h"
#include "../utils/string.h"

#include <fstream> // write to file

FILE* binaryFVMeshBlob = nullptr;

/* Custom fgets function to support Mac line-ends in Ascii STL files. OpenSCAD produces this when used on Mac */
void* fgets_(char* ptr, size_t len, FILE* f)
{
    while(len && fread(ptr, 1, 1, f) > 0)
    {
        if (*ptr == '\n' || *ptr == '\r')
        {
            *ptr = '\0';
            return ptr;
        }
        ptr++;
        len--;
    }
    return nullptr;
}

bool loadModelSTL_ascii(FVMesh* mesh, const char* filename, FMatrix3x3& matrix)
{
    FILE* f = fopen(filename, "rt");
    char buffer[1024];
    FPoint3 vertex;
    int n = 0;
    Point3 v0(0,0,0), v1(0,0,0), v2(0,0,0);
    while(fgets_(buffer, sizeof(buffer), f))
    {
        if (sscanf(buffer, " vertex %f %f %f", &vertex.x, &vertex.y, &vertex.z) == 3)
        {
            n++;
            switch(n)
            {
            case 1:
                v0 = matrix.apply(vertex);
                break;
            case 2:
                v1 = matrix.apply(vertex);
                break;
            case 3:
                v2 = matrix.apply(vertex);
                mesh->addFace(v0, v1, v2);
                n = 0;
                break;
            }
        }
    }
    fclose(f);
    mesh->finish();
    return true;
}

bool loadModelSTL_binary(FVMesh* mesh, const char* filename, FMatrix3x3& matrix)
{
    FILE* f = fopen(filename, "rb");
    char buffer[80];
    uint32_t faceCount;
    //Skip the header
    if (fread(buffer, 80, 1, f) != 1)
    {
        fclose(f);
        return false;
    }
    //Read the face count
    if (fread(&faceCount, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        return false;
    }
    //For each face read:
    //float(x,y,z) = normal, float(X,Y,Z)*3 = vertexes, uint16_t = flags
    for(unsigned int i=0;i<faceCount;i++)
    {
        if (fread(buffer, sizeof(float) * 3, 1, f) != 1)
        {
            fclose(f);
            return false;
        }
        float v[9];
        if (fread(v, sizeof(float) * 9, 1, f) != 1)
        {
            fclose(f);
            return false;
        }
        Point3 v0 = matrix.apply(FPoint3(v[0], v[1], v[2]));
        Point3 v1 = matrix.apply(FPoint3(v[3], v[4], v[5]));
        Point3 v2 = matrix.apply(FPoint3(v[6], v[7], v[8]));
        mesh->addFace(v0, v1, v2);
        if (fread(buffer, sizeof(uint16_t), 1, f) != 1)
        {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    mesh->finish();
    return true;
}

bool loadModelSTL(FVMesh* mesh, const char* filename, FMatrix3x3& matrix)
{
    FILE* f = fopen(filename, "r");
    char buffer[6];
    if (f == nullptr)
        return false;

    if (fread(buffer, 5, 1, f) != 1)
    {
        fclose(f);
        return false;
    }
    fclose(f);

    buffer[5] = '\0';
    if (stringcasecompare(buffer, "solid") == 0)
    {
        bool load_success = loadModelSTL_ascii(mesh, filename, matrix);
        if (!load_success)
            return false;

        // This logic is used to handle the case where the file starts with
        // "solid" but is a binary file.
        if (mesh->faces.size() < 1)
        {
            mesh->clear();
            return loadModelSTL_binary(mesh, filename, matrix);
        }
        return true;
    }
    return loadModelSTL_binary(mesh, filename, matrix);
}

bool loadFVMeshFromFile(PrintObject* object, const char* filename, FMatrix3x3& matrix)
{
    const char* ext = strrchr(filename, '.');
    if (ext && strcmp(ext, ".stl") == 0)
    {
        object->meshes.emplace_back(object);
        return loadModelSTL(&object->meshes[object->meshes.size()-1], filename, matrix);
    }
    return false;
}

bool saveFVMeshToFile(FVMesh& mesh, const char* filename)
{
    std::ofstream out(filename);
    out << "solid name" << std::endl;

    auto getPoint = [&mesh](int f, int v) { return &mesh.vertices[mesh.faces[f].vertex_index[v]].p; } ;

    Point3* vert;
    for (int f = 0; f < mesh.faces.size() ; f++)
    {

        out << "facet normal 0 0 0" << std::endl;
        out << "    outer loop" << std::endl;
        vert = getPoint(f,0);
        out << "        vertex " <<vert->x <<" "<<vert->y<<" "<<vert->z  << std::endl;
        vert = getPoint(f,1);
        out << "        vertex " <<vert->x <<" "<<vert->y<<" "<<vert->z  << std::endl;
        vert = getPoint(f,2);
        out << "        vertex " <<vert->x <<" "<<vert->y<<" "<<vert->z  << std::endl;
        out << "    endloop" << std::endl;
        out << "endfacet" << std::endl;

    }

    out << "endsolid name" << std::endl;
    out.close();

    return true;
}

