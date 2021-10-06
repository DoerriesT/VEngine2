-- script1.lua

local MyAnimGraphScript = 
{
    Properties =
    {
        -- Property definitions
    }
}

function iterateCallback(transformComp, entity, movementComp)
	print(entity .. ' ' .. transformComp.m_translationX .. ' ' .. transformComp.m_translationY .. ' ' .. transformComp.m_translationZ .. ' ' .. movementComp.m_velocityX .. ' ' .. movementComp.m_velocityZ)
end

function MyAnimGraphScript:update(graph, ecs, entity, deltaTime)

    local mc = ecs:getComponent(entity, ComponentIDs['CharacterMovementComponent'])
	local speed = math.sqrt(mc.m_velocityX * mc.m_velocityX + mc.m_velocityZ * mc.m_velocityZ)
	graph:setFloatParam('speed', speed)
	
end

return MyAnimGraphScript