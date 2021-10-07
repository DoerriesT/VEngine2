-- anim_graph.lua

local AnimGraphScript = 
{
    Properties =
    {
        -- Property definitions
    }
}

function AnimGraphScript:update(graph, ecs, entity, deltaTime)

    local mc = ecs:getComponent(entity, ComponentIDs['CharacterMovementComponent'])
	local speed = math.sqrt(mc.m_velocityX * mc.m_velocityX + mc.m_velocityZ * mc.m_velocityZ)
	graph:setFloatParam('speed', speed)
	
end

return AnimGraphScript