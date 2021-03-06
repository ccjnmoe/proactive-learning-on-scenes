// Copyright 2014 Isis Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../../Utils/ITMLibDefines.h"
#include "../../Utils/ITMPixelUtils.h"
#include "ITMSceneReconstructionEngine.h"
#include <iostream>

// sigma that controls the basin of attraction
#define DTUNE 6.0f

_CPU_AND_GPU_CODE_ inline void unprojectPtWithIntrinsic(const Vector4f &intrinsic, const Vector3f &inpt, Vector4f &outpt)
{
	outpt.x = intrinsic.x * inpt.x + intrinsic.z * inpt.z;
	outpt.y = intrinsic.y * inpt.y + intrinsic.w * inpt.z;
	outpt.z = inpt.z;
	outpt.w = 1.0f;
}

template<class TVoxel, class TIndex>
_CPU_AND_GPU_CODE_ inline float computePerPixelEnergy(const Vector4f &inpt, const TVoxel *voxelBlocks, const typename TIndex::IndexData *index,
	float oneOverVoxelSize, Matrix4f invM)
{
	Vector3f pt; bool dtIsFound;
	pt = (invM * inpt * oneOverVoxelSize).toVector3();
	float dt = readFromSDF_float_uninterpolated(voxelBlocks, index, pt, dtIsFound);

	if (dt == 1.0f) return 0.0f;

	float expdt = expf(-dt * DTUNE);
	return 4.0f * expdt / ((expdt + 1.0f)*(expdt + 1.0f));
}

template<class TVoxel, class TIndex>
_CPU_AND_GPU_CODE_ inline Vector3f computeDDT(const Vector3f &pt_f, const TVoxel *voxelBlocks, const typename TIndex::IndexData *index,
	float oneOverVoxelSize, bool &ddtFound)
{
	Vector3f ddt;
	
	Vector3i pt = pt_f.toIntRound();
	
	bool isFound; float dt1, dt2;	

	dt1 = TVoxel::SDF_valueToFloat(readVoxel(voxelBlocks, index, pt + Vector3i(1, 0, 0), isFound).sdf);
	if (!isFound || dt1 == 1.0f) { ddtFound = false; return Vector3f(0.0f); }
	dt2 = TVoxel::SDF_valueToFloat(readVoxel(voxelBlocks, index, pt + Vector3i(-1, 0, 0), isFound).sdf);
	if (!isFound || dt2 == 1.0f) { ddtFound = false; return Vector3f(0.0f); }
	ddt.x = (dt1 - dt2) * 0.5f;

	dt1 = TVoxel::SDF_valueToFloat(readVoxel(voxelBlocks, index, pt + Vector3i(0, 1, 0), isFound).sdf);
	if (!isFound || dt1 == 1.0f) { ddtFound = false; return Vector3f(0.0f); }
	dt2 = TVoxel::SDF_valueToFloat(readVoxel(voxelBlocks, index, pt + Vector3i(0, -1, 0), isFound).sdf);
	if (!isFound || dt2 == 1.0f) { ddtFound = false; return Vector3f(0.0f); }
	ddt.y = (dt1 - dt2) * 0.5f;

	dt1 = TVoxel::SDF_valueToFloat(readVoxel(voxelBlocks, index, pt + Vector3i(0, 0, 1), isFound).sdf);
	if (!isFound || dt1 == 1.0f) { ddtFound = false; return Vector3f(0.0f); }
	dt2 = TVoxel::SDF_valueToFloat(readVoxel(voxelBlocks, index, pt + Vector3i(0, 0, -1), isFound).sdf);
	if (!isFound || dt2 == 1.0f) { ddtFound = false; return Vector3f(0.0f); }
	ddt.z = (dt1 - dt2) * 0.5f;

	ddtFound = true; return ddt;
}

template<class TVoxel, class TIndex>
_CPU_AND_GPU_CODE_ inline bool computePerPixelJacobian(float *jacobian, const Vector4f &inpt, const TVoxel *voxelBlocks, const typename TIndex::IndexData *index,
	float oneOverVoxelSize, Matrix4f invM)
{
	float dt;

	bool isFound;

	Vector3f cPt, dDt, pt;

	cPt = (invM * inpt).toVector3();

	pt = cPt * oneOverVoxelSize;

	dt = readFromSDF_float_uninterpolated(voxelBlocks, index, pt, isFound);

	if (dt == 1.0f || !isFound) return false;


	dDt = computeDDT<TVoxel, TIndex>(pt, voxelBlocks, index, oneOverVoxelSize, isFound);
	if (!isFound) return false;

	float expdt = expf(-dt * DTUNE);
	float deto = expdt + 1;

	float prefix = 4.0f * DTUNE * (2.0f * expf(-dt * 2.0f * DTUNE) / (deto * deto * deto) - expdt / (deto * deto));

	dDt *= prefix;

	jacobian[0] = dDt.x; jacobian[1] = dDt.y; jacobian[2] = dDt.z;

	jacobian[3] = 4.0f * (dDt.z * cPt.y - dDt.y * cPt.z);
	jacobian[4] = 4.0f * (dDt.x * cPt.z - dDt.z * cPt.x);
	jacobian[5] = 4.0f * (dDt.y * cPt.x - dDt.x * cPt.y);

	return true;
}
