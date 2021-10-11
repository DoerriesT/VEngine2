-- scripted_sphere.lua

local SphereScript = 
{
    Properties =
    {
		MinHeight = 0.0,
		MaxHeight = 3.0,
        Speed = 1.0,
		Active = true
    }
}

moveUp = true
printed = false

function SphereScript:update(ecs, entity, deltaTime)
	
	if (self.Properties.Active) then
		local tc = ecs:getComponent(entity, ComponentIDs['TransformComponent'])
		
		if (tc.m_translationY > self.Properties.MaxHeight) then
			moveUp = false
		elseif (tc.m_translationY < self.Properties.MinHeight) then
			moveUp = true
		end
		
		mult = 10.0
		
		if (moveUp) then
			tc.m_translationY = tc.m_translationY + deltaTime * self.Properties.Speed * mult
		else
			tc.m_translationY = tc.m_translationY - deltaTime * self.Properties.Speed * mult
		end
	
		ecs:setComponent(entity, ComponentIDs['TransformComponent'], tc)
	end
	
end

return SphereScript