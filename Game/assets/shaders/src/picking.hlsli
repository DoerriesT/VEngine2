#ifndef PICKING_H
#define PICKING_H

// TODO: use atomics to guard against concurrent access
void updatePickingBuffer(RWByteAddressBuffer pickingBuffer, uint2 pixelCoord, uint2 pickingPos, uint entityID, float depth)
{
	// early out if this pixel is not under the mouse cursor
	if (any(pixelCoord != pickingPos))
	{
		return;
	}
	
	uint depthUI = asuint(depth);
	
	// early out if our depth is farther away than the one stored in the buffer. note the reversed depth buffer!
	if (depthUI <= pickingBuffer.Load(0))
	{
		return;
	}
	
	pickingBuffer.Store2(0, uint2(depthUI, entityID));
}

#endif // PICKING_H