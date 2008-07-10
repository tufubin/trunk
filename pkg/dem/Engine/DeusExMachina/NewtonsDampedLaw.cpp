/*************************************************************************
 Copyright (C) 2008 by Bruno Chareyre		                         *
*  bruno.chareyre@hmg.inpg.fr      					 *
*                                                                        *
*  This program is free software; it is licensed under the terms of the  *
*  GNU General Public License v2 or later. See file LICENSE for details. *
*************************************************************************/

#include "NewtonsDampedLaw.hpp"
#include<yade/core/MetaBody.hpp>
#include <yade/pkg-common/RigidBodyParameters.hpp>
#include <yade/pkg-common/Momentum.hpp>
#include <yade/pkg-common/Force.hpp>
#include<yade/lib-base/yadeWm3Extra.hpp>


void NewtonsDampedLaw::registerAttributes()
{
	//DeusExMachina::registerAttributes();
	REGISTER_ATTRIBUTE(damping);
}


NewtonsDampedLaw::NewtonsDampedLaw()
{
	damping = 0.2;
	forceClassIndex = (new Force)->getClassIndex();
	momentumClassIndex = (new Momentum)->getClassIndex();
}

void NewtonsDampedLaw::applyCondition ( MetaBody * ncb )
{
	shared_ptr<BodyContainer>& bodies = ncb->bodies;
	BodyContainer::iterator bi    = bodies->begin();
	BodyContainer::iterator biEnd = bodies->end();
	for ( ; bi!=biEnd ; ++bi )
	{

		const shared_ptr<Body>& b = *bi;

		if (!b->isDynamic) continue;
		
		RigidBodyParameters * rb = YADE_CAST<RigidBodyParameters*> ( b->physicalParameters.get() );
		unsigned int id = b->getId();
		Vector3r& m = ( static_cast<Momentum*> ( ncb->physicalActions->find ( id, momentumClassIndex ).get() ) )->momentum;
		Vector3r& f = ( static_cast<Force*> ( ncb->physicalActions->find ( id, forceClassIndex ).get() ) )->force;

		Real dt = Omega::instance().getTimeStep();

		



		//Newtons mometum law :
		if ( b->isStandalone() ) rb->angularAcceleration=diagDiv ( m,rb->inertia );
		else if ( b->isClump() ) rb->angularAcceleration+=diagDiv ( m,rb->inertia );
		else
		{ // isClumpMember()
			const shared_ptr<Body>& clump ( Body::byId ( b->clumpId ) );
			RigidBodyParameters* clumpRBP=YADE_CAST<RigidBodyParameters*> ( clump->physicalParameters.get() );
			/* angularAcceleration is reset by ClumpMemberMover engine */
			clumpRBP->angularAcceleration+=diagDiv ( m,clumpRBP->inertia );
		}

		// Newtons force law
		if ( b->isStandalone() ) rb->acceleration=f/rb->mass;
		else if ( b->isClump() )
		{
			// accel for clump reset in Clump::moveMembers, called by ClumpMemberMover engine
			rb->acceleration+=f/rb->mass;
		}
		else
		{ // force applied to a clump member is applied to clump itself
			const shared_ptr<Body>& clump ( Body::byId ( b->clumpId ) );
			RigidBodyParameters* clumpRBP=YADE_CAST<RigidBodyParameters*> ( clump->physicalParameters.get() );
			// accels reset by Clump::moveMembers in last iteration
			clumpRBP->acceleration+=f/clumpRBP->mass;
			clumpRBP->angularAcceleration+=diagDiv ( ( rb->se3.position-clumpRBP->se3.position ).Cross ( f ),clumpRBP->inertia ); //acceleration from torque generated by the force WRT particle centroid on the clump centroid
		}


		if (!b->isClump()) {
			//Damping moments
			for ( int i=0; i<3; ++i )
			{
				rb->angularAcceleration[i] *= 1 - damping*Mathr::Sign ( m[i]*(rb->angularVelocity[i] + (Real) 0.5 *dt*rb->angularAcceleration[i]) );
			}
			//Damping force :
			for ( int i=0; i<3; ++i )
			{
				rb->acceleration[i] *= 1 - damping*Mathr::Sign ( f[i]*(rb->velocity[i] + (Real) 0.5 *dt*rb->acceleration[i]) );
			}
		}

		rb->angularVelocity = rb->angularVelocity+ dt*rb->angularAcceleration;
		rb->velocity = rb->velocity+ dt*rb->acceleration;

		Vector3r axis = rb->angularVelocity;
		Real angle = axis.Normalize();
		Quaternionr q;
		q.FromAxisAngle ( axis,angle*dt );
		rb->se3.orientation = q*rb->se3.orientation;
		rb->se3.orientation.Normalize();


		rb->se3.position += rb->velocity*dt;
	}
}

YADE_PLUGIN();