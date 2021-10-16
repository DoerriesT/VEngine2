-- anim_graph.lua

local AnimGraphScript = 
{
    Properties =
    {
        -- Property definitions
    }
}

local speedHistory = 0.0

function AnimGraphScript:update(graph, ecs, entity, deltaTime)

    local mc = ecs:getComponent(entity, ComponentIDs['CharacterMovementComponent'])
	local speed = math.sqrt(mc.m_velocityX * mc.m_velocityX + mc.m_velocityZ * mc.m_velocityZ)
	local alpha = 0.15
	speedHistory = speed * alpha + speedHistory * (1.0 - alpha)
	graph:setFloatParam('speed', speedHistory)
	
end

return AnimGraphScript