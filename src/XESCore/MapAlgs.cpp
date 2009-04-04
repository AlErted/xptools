/*
 * Copyright (c) 2004, Laminar Research.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include "MapAlgs.h"
#include "MapTopology.h"
#include "ParamDefs.h"
#include "GISUtils.h"
#include <iomanip>
#include "AssertUtils.h"
#include "CompGeomUtils.h"
#include "PolyRasterUtils.h"
#include "MapOverlay.h"
#include "DEMDefs.h"
#include "PerfUtils.h"
#include "MapHelpers.h"

// NOTE: by convention all of the static helper routines and structs have the __ prefix..this is intended
// for the sole purpose of making it easy to read the function list popup in the IDE...


// Show all ideal insert lines for an inset!
#define DEBUG_SHOW_INSET 0
#define TRACE_TOPO_INTEGRATE 1

#if DEBUG_SHOW_INSET
#include "WED_Globals.h"
#endif

void	RebuildMap(
			Pmwx&			in_map,
			Pmwx&			out_map)
{
	out_map.clear();
	vector<X_monotone_curve_2>	curves;
	curves.reserve(in_map.number_of_halfedges() / 2);
	for(Pmwx::Edge_iterator i = in_map.edges_begin(); i != in_map.edges_end(); ++i)
	{
		curves.push_back(i->curve());
	}
	printf("Rebuilding %d edges.\n",curves.size());
	StElapsedTime rebuild_map("Rebuild_map");
	CGAL::insert_curves(out_map,curves.begin(),curves.end());	
}

/*
void	CCBToPolygon(Halfedge_const_handle ccb, Polygon2& outPolygon, vector<double> * road_types, double (* weight_func)(Halfedge_const_handle edge), Bbox2 * outBounds)
{
	if (road_types != NULL) DebugAssert(weight_func != NULL);
	if (road_types == NULL) DebugAssert(weight_func == NULL);

	outPolygon.clear();
	if (road_types) road_types->clear();
	
	Halfedge_const_handle iter = ccb, stop = ccb;
	
	if (outBounds)	(*outBounds) = cgal2ben(iter->source()->point());
	
	do {
		// BEN SAYS: SWITCH TO SOURCE or LU type is off by one!
		#if !DEV
		hello
		#endif
		outPolygon.push_back(cgal2ben(iter->target()->point()));
		if (outBounds) (*outBounds) += cgal2ben(iter->target()->point());
		if (road_types)
			road_types->push_back(weight_func(iter));
		iter = iter->next();
	} while (iter != stop);
}

void	FaceToComplexPolygon(Face_const_handle face, vector<Polygon2>& outPolygon, vector<vector<double> > * road_types, double (* weight_func)(Halfedge_const_handle edge), Bbox2 * outBounds)
{
	outPolygon.clear();
	if (road_types)	road_types->clear();

	if (!face->is_unbounded())
	{
		outPolygon.push_back(Polygon2());
		if (road_types) road_types->push_back(vector<double>());
		CCBToPolygon(face->outer_ccb(), outPolygon.back(), road_types ? &road_types->back() : NULL, weight_func, outBounds);
	}
	
	for (Pmwx::Hole_const_iterator hole = face->holes_begin(); hole != face->holes_end(); ++hole)	
	{
		outPolygon.push_back(Polygon2());
		if (road_types) road_types->push_back(vector<double>());
		CCBToPolygon(*hole, outPolygon.back(), road_types ? &road_types->back() : NULL, weight_func, NULL);
	}
}

Face_handle	ComplexPolygonToPmwx(const vector<Polygon2>& inPolygons, Pmwx& outPmwx, int inTerrain, int outTerrain)
{
	Face_handle outer = Face_handle();
	outPmwx.clear();
	outPmwx.unbounded_face()->data().mTerrainType = outTerrain;
	for (vector<Polygon2>::const_iterator poly = inPolygons.begin(); poly != inPolygons.end(); ++poly)
	{
		Face_handle parent = (poly == inPolygons.begin()) ? outPmwx.unbounded_face() : outer;
		if (poly == inPolygons.begin())
		{
			Face_handle new_f = SafeInsertRing(&outPmwx, parent, *poly);
			outer = new_f;
			new_f->data().mTerrainType = inTerrain;
		
		}
		else
		{
			Polygon2	rev(*poly);
			reverse(rev.begin(), rev.end());
			Face_handle new_f = SafeInsertRing(&outPmwx, parent, rev);
			new_f->data().mTerrainType = outTerrain;
		}
	}
	return outer;
}

*/
/************************************************************************************************
 * MAP EDITING
 ************************************************************************************************/
#pragma mark -

static void	CutInside(
			Pmwx&				ioMap,
			const Polygon_2&	inBoundary,
			bool				inWantOutside,
			ProgressFunc		inProgress);

void	CropMap(
			Pmwx&			ioMap,
			double			inWest,
			double			inSouth,
			double			inEast,
			double			inNorth,
			bool			inKeepOutside,	// If true, keep outside crop zone (cut a hole), otherwise keep only inside (normal crop)
			ProgressFunc	inProgress)
{
	Polygon_2	p;
	p.push_back(Point_2(inWest,inSouth));
	p.push_back(Point_2(inEast,inSouth));
	p.push_back(Point_2(inEast,inNorth));
	p.push_back(Point_2(inWest,inNorth));
	CutInside(ioMap, p, inKeepOutside,inProgress);
}

void	CropMap(
			Pmwx&					ioMap,
			Pmwx&					outCutout,
			const vector<Point_2>&	inRingCCW,
			ProgressFunc			inProgress)
{
	outCutout = ioMap;
	Polygon_2 p(inRingCCW.begin(),inRingCCW.end());
	CutInside(ioMap,p,true,inProgress);
	CutInside(outCutout,p,false,inProgress);
}

class	collect_edges : public data_preserver_t {
public:
	set<Halfedge_handle> *  edges;
	bool					use_outside;

  virtual void after_create_edge (Halfedge_handle e)
  {
		bool reverse = false;
//		printf("Created edge 0x%08x/0x%08x ",&*e,&*e->twin());
		Vector_2	actual_dir(e->source()->point(),e->target()->point());
		
		if(use_outside)
		{
			if(CGAL::angle(e->curve().target(),e->curve().source(),e->curve().source()+actual_dir)==CGAL::ACUTE)
				reverse = true;
			else
				reverse = false;
		}
		else
		{		
			if(CGAL::angle(e->curve().target(),e->curve().source(),e->curve().source()+actual_dir)==CGAL::ACUTE)
				reverse = false;
			else
				reverse = true;
		}

		if(reverse)
				edges->insert(e->twin());
			else
				edges->insert(e);
				
//		printf("Got edge: %lf,%lf->%lf,%lf, reverse=%s\n",
//			CGAL::to_double(e->source()->point().x()),
//			CGAL::to_double(e->source()->point().y()),
//			CGAL::to_double(e->target()->point().x()),
//			CGAL::to_double(e->target()->point().y()),
//			reverse ? "yes" : "no");
		
		
//		DebugAssert(e->		data().mDominant != e->twin()->data().mDominant);
		
//		e->twin()->data().mDominant=1;
		
  }
  
    virtual void after_split_edge (Halfedge_handle  e1,
                                 Halfedge_handle  e2)
	{
		data_preserver_t::after_split_edge(e1,e2);
//		printf("Split edge: 0x%08x/0x%08x   to make 0x%08x/0x%08x %lf,%lf->%lf,%lf and %lf,%lf->%lf,%lf\n",
//			&*e1,&*e1->twin(),&*e2,&*e2->twin(),
//			CGAL::to_double(e1->source()->point().x()),
//			CGAL::to_double(e1->source()->point().y()),
//			CGAL::to_double(e1->target()->point().x()),
//			CGAL::to_double(e1->target()->point().y()),
//
//			CGAL::to_double(e2->source()->point().x()),
//			CGAL::to_double(e2->source()->point().y()),
//			CGAL::to_double(e2->target()->point().x()),
//			CGAL::to_double(e2->target()->point().y()));
	
	
	
//		DebugAssert(!e2->data().mDominant);
//		DebugAssert(!e2->twin()->data().mDominant);
//		DebugAssert(e1->data().mDominant != e1->twin()->data().mDominant);
//		e2->data().mDominant = e1->data().mDominant;
//		e2->twin()->data().mDominant = e1->twin()->data().mDominant;
//		DebugAssert(edges->count(e2) == 0);
//		DebugAssert(edges->count(e2->twin()) == 0);

//		DebugAssert(e1->data().mDominant != e1->twin()->data().mDominant);
//		DebugAssert(e2->data().mDominant != e2->twin()->data().mDominant);
		if(edges->count(e1)) edges->insert(e2);
		if(edges->count(e1->twin())) edges->insert(e2->twin());

	}
};



void	CutInside(
			Pmwx&				ioMap,
			const Polygon_2&	inBoundary,
			bool				inWantOutside,
			ProgressFunc		inProgress)
{
	set<Halfedge_handle>	edges;
	collect_edges		info;
	info.edges = &edges;
	info.use_outside = false;
	
	info.attach(ioMap);
	
//	DebugAssert(CGAL::is_valid(ioMap));
	DebugAssert(inBoundary.is_simple());
	ValidateMapDominance(ioMap);
	vector<X_monotone_curve_2> v(inBoundary.edges_begin(),inBoundary.edges_end());
//	CGAL::insert_x_monotone_curves(ioMap,v.begin(),v.end());
	CGAL::insert_curves(ioMap,v.begin(),v.end());

//	DebugAssert(CGAL::is_valid(ioMap));
	DebugAssert(inBoundary.is_simple());
	ValidateMapDominance(ioMap);

/*
	for(vector<X_monotone_curve_2>::iterator i = v.begin(); i != v.end(); ++i)
	{
		printf("INSERTING: %lf,%f->%lf,%lf\n",
				CGAL::to_double(i->source().x()),
				CGAL::to_double(i->source().y()),
				CGAL::to_double(i->target().x()),
				CGAL::to_double(i->target().y()));
		CGAL::insert_curve(ioMap,*i);
	}
*/	
	info.detach();

		set<Face_handle>	interior_region;
		
	FindFacesForEdgeSet(edges,interior_region);
	
	set<Halfedge_handle>	all_interior_edges;

	for(set<Face_handle>::iterator f = interior_region.begin(); f != interior_region.end(); ++f)
	{
		DebugAssert(!(*f)->is_unbounded());
		FindEdgesForFace(*f,all_interior_edges);
	}

	for(set<Halfedge_handle>::iterator e = edges.begin(); e != edges.end(); ++e)
		DebugAssert(all_interior_edges.count(*e) > 0);

	vector<Halfedge_handle>	kill;
	
//	if(!inWantOutside)
	{
		// this is the case where we THROW OUT the OUTSIDE - still very slow.
		for(Pmwx::Edge_iterator e = ioMap.edges_begin(); e != ioMap.edges_end(); ++e)
		{
			CGAL::Bounded_side v1 = inBoundary.bounded_side(e->source()->point());
			CGAL::Bounded_side v2 = inBoundary.bounded_side(e->target()->point());
			
			// Sanity check - having burned in our edge, no edge should now SPAN our polygon.
			if (v1 == CGAL::ON_BOUNDED_SIDE) DebugAssert(v2 != CGAL::ON_UNBOUNDED_SIDE);
			if (v2 == CGAL::ON_BOUNDED_SIDE) DebugAssert(v1 != CGAL::ON_UNBOUNDED_SIDE);
			
			if(inWantOutside)
			if(v1 == CGAL::ON_BOUNDED_SIDE || v2 == CGAL::ON_BOUNDED_SIDE)	kill.push_back(e);

			if(!inWantOutside)
			if(v1 == CGAL::ON_UNBOUNDED_SIDE || v2 == CGAL::ON_UNBOUNDED_SIDE)	kill.push_back(e);
		}
	}
//	else
/*
	if(inWantOutside)
	{
		for(set<Halfedge_handle>::iterator e = all_interior_edges.begin(); e != all_interior_edges.end(); ++e)
		if(edges.count(*e) == 0)
		if((*e)->data().mDominant)
			if(find(kill.begin(),kill.end(),*e) == kill.end())
			{
				CGAL::Bounded_side v1 = inBoundary.bounded_side((*e)->source()->point());
				CGAL::Bounded_side v2 = inBoundary.bounded_side((*e)->target()->point());
				Halfedge_handle ee(*e);
				printf("Lost edge: 0x%08x/0x%08x %lf,%lf->%lf,%lf  is %d,%d\n",
						&*ee,&*ee->twin(),
						CGAL::to_double((*e)->source()->point().x()),
						CGAL::to_double((*e)->source()->point().y()),
						CGAL::to_double((*e)->target()->point().x()),
						CGAL::to_double((*e)->target()->point().y()),
						v1,v2);
			}

	}	
*/	
	for(vector<Halfedge_handle>::iterator k = kill.begin(); k != kill.end(); ++k)
		ioMap.remove_edge(*k);
//	printf("Finishde one cut inside.\n");
	
//	DebugAssert(CGAL::is_valid(ioMap));
	DebugAssert(inBoundary.is_simple());
	ValidateMapDominance(ioMap);
	
}

#if 0
// This notifier and accompanying struct accumlates all halfedges that are inserted or found
// doing an insert-with intersections as well as all target points in order.
struct	__CropProgInfo_t {
	vector<Point_2>			pts;
	vector<Halfedge_handle>	edges;
};

static void	__ProgNotifier(Halfedge_handle o, Halfedge_handle n, void * r)
{
	__CropProgInfo_t * ref = (__CropProgInfo_t *) r;
	Halfedge_handle e = (n == Halfedge_handle()) ? o : n;
	
	if (o == Halfedge_handle() || n == Halfedge_handle())
	if (o != Halfedge_handle() || n != Halfedge_handle())
	{
		DebugAssert(e != Halfedge_handle());
		ref->edges.push_back(e);
		ref->pts.push_back(e->target()->point());
	}
}

// Basic crop map.  Build a ring from our bounds and call advanced crop-map.
void	CropMap(
			Pmwx&			ioMap,
			double			inWest,
			double			inSouth,
			double			inEast,
			double			inNorth,
			bool			inKeepOutside,
			ProgressFunc	inFunc)
{
	Pmwx	cutout;
	
	vector<Point_2>	pts;
	pts.push_back(Point_2(inWest, inNorth));
	pts.push_back(Point_2(inWest, inSouth));
	pts.push_back(Point_2(inEast, inSouth));
	pts.push_back(Point_2(inEast, inNorth));
	CropMap(ioMap, cutout, pts, inFunc);
	//if (!inKeepOutside)
	//	ioMap.swap(cutout);
}


// Advanced crop map.  Insert the crop ring into the main map with intersections
// and into the cutout empty map using the fast "insert_ring" primitive.
// Then swap to get the insides into the cutout ring.


void	CropMap(
			Pmwx&					ioMap,
			Pmwx&					ioCutout,
			const vector<Point_2>&	ring,			
			ProgressFunc			inFunc)
{
	__CropProgInfo_t	info;
	__CropProgInfo_t * ref = &info;

	vector<X_monotone_curve_2> curves;
	
	for (int n = 0; n < ring.size(); ++n)
	{
		int m = (n+1)%ring.size();
		if (inFunc) inFunc(0,2,"Cutting map...", (float) n / (float) ring.size());
		curves.push_back(X_monotone_curve_2(ring[n], ring[m]));
	}
	ioMap.insert_curves(curves.begin(), curves.end(), __ProgNotifier, &info);

	if (inFunc) inFunc(0, 2, "Cutting map...", 1.0);

	ioCutout.clear();
	Face_handle area;
	area = ioCutout.insert_ring(ioCutout.unbounded_face(), info.pts);
	vector<Halfedge_handle>	inner_ring;
	
	Pmwx::Ccb_halfedge_circulator stop, iter;
	stop = iter = area->outer_ccb();
	do {
		inner_ring.push_back(iter);
		++iter;
	} while (iter != stop);

	SwapMaps(ioMap, ioCutout, info.edges, inner_ring);
#if DEV
//	DebugAssert(ioMap.is_valid());
//	DebugAssert(ioCutout.is_valid());
#endif	

}


// Utility routine - delete antennas from a face.
static	void	__RemoveAntennasFromFace(Pmwx& inPmwx, Face_handle inFace)
{
// OPTIMIZE: remove second set?
	set<Halfedge_handle>	edges;
	set<Halfedge_handle>	nuke;
	
	FindEdgesForFace(inFace, edges);
	for (set<Halfedge_handle>::iterator i = edges.begin(); i != edges.end(); ++i)
	if ((*i)->twin()->face() == inFace)
	if(nuke.count((*i)->twin()->face)==0)
		nuke.insert(*i);

	for (set<Halfedge_handle>::iterator j = nuke.begin(); j != nuke.end(); ++j)
	{
		inPmwx.remove_edge(*j);
	}
}

// Utility: given a face CCB with no antennas, force the exact edges in
// two maps and record the vectors in order.  This uses a notifier to
// capture a series of inserts-with-intersections.
struct	__InduceCCBInfo_t {
	Halfedge_handle			master;
	vector<Halfedge_handle>	slave_halfedges;
	vector<Point_2>			break_pts;
};	

void	__InduceCCBNotifier(Halfedge_handle o, Halfedge_handle n, void * r)
{
	__InduceCCBInfo_t *	ref = (__InduceCCBInfo_t *) r;
	if ((o != Halfedge_handle()) && (n != Halfedge_handle()))
	{

	} else {
		Halfedge_handle e = (o == Halfedge_handle()) ? o : n;
		ref->slave_halfedges.push_back(e);
		if (e->source()->point() != ref->master->source()->point())
			ref->break_pts.push_back(e->source()->point());
	}
}

static	void	__InduceCCB(
					Pmwx& 					inMaster, 
					Pmwx& 					inSlave, 
					Halfedge_handle 			ccb, 
					vector<Halfedge_handle>& 	masterE, 
					vector<Halfedge_handle>& 	slaveE, 
					bool 					outer)
{
	Halfedge_handle iter = ccb;
	Halfedge_handle last_slave = Halfedge_handle();
	do {
		__InduceCCBInfo_t	info;
		info.master = iter;
		if (last_slave == Halfedge_handle())
			last_slave = inSlave.insert_edge(iter->source()->point(), iter->target()->point(), last_slave, Pmwx::locate_Vertex, __InduceCCBNotifier, &info);
		else
			last_slave = inSlave.insert_edge(iter->source()->point(), iter->target()->point(), __InduceCCBNotifier, &info);

		slaveE.insert(slaveE.end(), info.slave_halfedges.begin(), info.slave_halfedges.end());

		masterE.push_back(info.master);
		for (vector<Point_2>::iterator pt = info.break_pts.begin(); pt != info.break_pts.end(); ++pt)
		{
			info.master = inMaster.split_edge(info.master, *pt)->next();
			masterE.push_back(info.master);
			iter = info.master;
		}

		iter = iter->next();
	} while (iter != ccb);

	if (!outer)
	{
		vector<Halfedge_handle>	masterR, slaveR;
		vector<Halfedge_handle>::reverse_iterator riter;
		for (riter = masterE.rbegin(); riter != masterE.rend(); ++riter)
			masterR.push_back((*riter)->twin());
		for (riter = slaveE.rbegin(); riter != slaveE.rend(); ++riter)
			slaveR.push_back((*riter)->twin());

		masterE.swap(masterR);
		slaveE.swap(slaveR);
	}
}

// Face swap - clean antenna (a requirement) and induce the face into the other map.  Then we can use
// our swap operator.
void	SwapFace(
			Pmwx&			inMaster,
			Pmwx&			inSlave,
			Face_handle		inFace,
			ProgressFunc	inFunc)
{
	__RemoveAntennasFromFace(inMaster, inFace);

	vector<Halfedge_handle>	ringMaster, ringSlave;
	__InduceCCB(inMaster, inSlave, inFace->outer_ccb(), ringMaster, ringSlave, true);
	DebugAssert(ringMaster.size() == ringSlave.size());
	SwapMaps(inMaster, inSlave, ringMaster, ringSlave);
	
	for (Pmwx::Hole_iterator hole = inFace->holes_begin(); hole != inFace->holes_end(); ++hole)
	{
		ringMaster.clear();
		ringSlave.clear();
		__InduceCCB(inSlave, inMaster, *hole, ringMaster, ringSlave, false);
		DebugAssert(ringMaster.size() == ringSlave.size());
		SwapMaps(inSlave, inMaster, ringMaster, ringSlave);
	}
}

#endif

#if 0
// Overlay notification: this notifier tracks insertion into one map and back-induces splits into
// the source map!
struct OverlayInfo_t {
	vector<Halfedge_handle>	dstRing;
	vector<Halfedge_handle>	srcRing;
	Halfedge_handle			curSrc;
	Pmwx *					curSrcMap;
};

static void __OverlayNotifier(Halfedge_handle o, Halfedge_handle n, void * r)
{
	OverlayInfo_t * ref = (OverlayInfo_t *) r;
	if (o != Halfedge_handle() && n != Halfedge_handle()) return;
	Halfedge_handle e = (n == Halfedge_handle()) ? o : n;
	
	if (e->target()->point() == ref->curSrc->target()->point())
	{
		// this is the last edge to be added - easy!
		ref->dstRing.push_back(e);
		ref->srcRing.push_back(ref->curSrc);
	} else {
		// the edge took multiple pieces in the other map.
		Halfedge_handle t = ref->curSrcMap->split_edge(ref->curSrc, e->target()->point());
		if (t != Halfedge_handle()) {
			ref->curSrc = t;
		ref->dstRing.push_back(e);
		ref->srcRing.push_back(ref->curSrc);		
			ref->curSrc = ref->curSrc->next();
		} else {
			// that point was already there
		ref->curSrc = ref->curSrc->next();
		}
	}
}

// Overlay - please note that this is NOT a merge, it is an "overwrite"...stuff under "inSrc" inside "inDst" goes away.
// Strategy: for each contiguous blob in inSrc (an area that will overwrite inDst) we induce this ring in the dest map,
// swap and the stuff we overwrite ends up in inSrc.

// TODO: it would be nice to explicitly handle antennas in inSrc's outer islands.
void OverlayMap(
			Pmwx& 	inDst,
			Pmwx& 	inSrc)
{
	int ctr = 0;
	for (Pmwx::Hole_iterator hole = inSrc.unbounded_face()->holes_begin(); hole != inSrc.unbounded_face()->holes_end(); ++hole, ++ctr)
	{
		OverlayInfo_t	info;
		info.curSrcMap = &inSrc;
		Pmwx::Ccb_halfedge_circulator iter, stop;
		iter = stop = *hole;
		vector<Halfedge_handle>	edges;
		do {
			DebugAssert(iter->twin()->face() != inSrc.unbounded_face());
			edges.push_back(iter->twin());
			++iter;
		} while (iter != stop);


		for (vector<Halfedge_handle>::reverse_iterator riter = edges.rbegin(); riter != edges.rend(); ++riter)
		{
			info.curSrc = *riter;
			inDst.insert_edge((*riter)->source()->point(), (*riter)->target()->point(), __OverlayNotifier, &info);
		}
#if DEV
//		DebugAssert(inSrc.is_valid());
//		DebugAssert(inDst.is_valid());
#endif
		SwapMaps(inDst, inSrc, info.dstRing, info.srcRing);
	}
}

#endif

#if 0
// Utility: this notifier establishes the mapping from the src to the dst as we insert!
struct __MergeMaps_EdgeNotifier_t {
	multimap<Halfedge_handle, Halfedge_handle> *		edgeMap;
	Halfedge_handle									srcEdge;
	map<Vertex_handle, Vertex_handle> *					vertMap;
};

static void __MergeMaps_EdgeNotifier(Halfedge_handle he_old, Halfedge_handle he_new, void * ref)
{
	__MergeMaps_EdgeNotifier_t * info = (__MergeMaps_EdgeNotifier_t *) ref;

	if (he_old != Halfedge_handle() && he_new != Halfedge_handle)
	{
		// This is a huge stinking mess...we have split an edge in the new map as we introduce lines.  But this means that
		// our mapping from old to new is no longer right.  Fix it!
		// (This case is not needed for topologically integrated maps but IS needed for not-meant-to-be-friends maps, like
		// a forest pattern + a road grid.

		vector<pair<GISHalfedge*, GISHalfedge*> >	new_mappings;
		for (multimap<GISHalfedge*, GISHalfedge*>::iterator pairing = info->edgeMap->begin(); pairing != info->edgeMap->end(); ++pairing)
		{
			if (pairing->second == he_old)
				AssertPrintf("WARNING: splits in the map merge.  This case is foolishly NOT handled yet!");
		}

	} else {
		// This is a new edge introduced...whether it is a real edge or a dupe of the old one, we need to copy params
		// and establish a linking.
		Halfedge_handle he = (he_old ? he_old : he_new);
		
		// Remember the mapping from old to new
		info->edgeMap->insert(multimap<Halfedge_handle, Halfedge_handle>::value_type(info->srcEdge, he));
		info->edgeMap->insert(multimap<Halfedge_handle, Halfedge_handle>::value_type(info->srcEdge->twin(), he->twin()));
		
		if (he->source()->point() == info->srcEdge->source()->point())
			(*info->vertMap)[info->srcEdge->source()] = he->source();

		if (he->target()->point() == info->srcEdge->source()->point())
			(*info->vertMap)[info->srcEdge->source()] = he->target();

		if (he->source()->point() == info->srcEdge->target()->point())
			(*info->vertMap)[info->srcEdge->target()] = he->source();

		if (he->target()->point() == info->srcEdge->target()->point())
			(*info->vertMap)[info->srcEdge->target()] = he->target();

		if (!he->mDominant) he = he->twin();

		// Copy the halfedge
		he->mSegments.insert(he->mSegments.end(), info->srcEdge->mSegments.begin(),info->srcEdge->mSegments.end());
		he->mParams.insert(info->srcEdge->mParams.begin(),info->srcEdge->mParams.end());
	}
}

void MergeMaps(Pmwx& ioDstMap, Pmwx& ioSrcMap, bool inForceProps, set<Face_handle> * outFaces, bool pre_integrated, ProgressFunc func)
{
// OPTIMIZE: now that maps have a point index, we do not need to maintain a prviate one!
	DebugAssert(ioSrcMap.is_valid());

	// Step 1 - we need to copy all halfedges from ioSrcMap to ioDstMap.
	// We'll remember the mappign and also copy attributes.

		multimap<Halfedge_handle, Halfedge_handle>		edgeMap;
		map<Vertex_handle, Vertex_handle> 				vertMap;
	__MergeMaps_EdgeNotifier_t info;
	info.edgeMap = &edgeMap;
	info.vertMap = &vertMap;

	int ctr = 0;
	int total = ioSrcMap.number_of_halfedges();
	int step = total / 200;

	PROGRESS_START(func, 0, 2, "Merging edges into map...")

	if (pre_integrated)
	{
		// Pre-integrated edge merge case - we know there are no intersections, so find points and use the fast insertion routines.
	
		map<Point_2, Vertex_handle, lesser_y_then_x>			pt_index;
		
		for (Pmwx::Vertex_iterator v = ioDstMap.vertices_begin(); v != ioDstMap.vertices_end(); ++v)
			pt_index[v->point()] = v;

		for (Pmwx::Halfedge_iterator iter = ioSrcMap.halfedges_begin(); iter != ioSrcMap.halfedges_end(); ++iter, ++ctr)
		if (iter->mDominant)
		{
			PROGRESS_CHECK(func, 0, 2, "Merging edges into map...", ctr, total, step)
		
			map<Point_2, Vertex_handle, lesser_y_then_x>::iterator i1, i2;
			Halfedge_handle nh;
			
			i1 = pt_index.find(iter->source()->point());
			i2 = pt_index.find(iter->target()->point());
			if (i1 != pt_index.end())
			{
				if (i2 != pt_index.end())
				{
					/* CASE 1 - Both points already in. */
					nh = ioDstMap.nox_insert_edge_between_vertices(i1->second, i2->second);
				}
				else
				{
					/* Case 2 - Point 1 in, point 2 new. */
					nh = ioDstMap.nox_insert_edge_from_vertex(i1->second, iter->target()->point());
					pt_index[iter->target()->point()] = nh->target();
				}
			}
			else
			{
				if (i2 != pt_index.end())
				{
					/* Case 3 - Point 1 new, point 2 in. */
					nh = ioDstMap.nox_insert_edge_from_vertex(i2->second, iter->source()->point())->twin();
					pt_index[iter->source()->point()] = nh->source();
				}
				else
				{
					/* Case 4 - both points new. */
					nh = ioDstMap.nox_insert_edge_in_hole(iter->source()->point(), iter->target()->point());
					pt_index[iter->source()->point()] = nh->source();
					pt_index[iter->target()->point()] = nh->target();
				}
			}
			
			edgeMap.insert(multimap<Halfedge_handle, Halfedge_handle>::value_type(iter, nh));
			edgeMap.insert(multimap<Halfedge_handle, Halfedge_handle>::value_type(iter->twin(), nh->twin()));
			if (!nh->mDominant) nh = nh->twin();
			nh->mSegments.insert(nh->mSegments.end(), iter->mSegments.begin(),iter->mSegments.end());
			nh->mParams.insert(iter->mParams.begin(),iter->mParams.end());

		}


	} else {

		// Slow case - use inserts with intersections.

		int fast = 0, slow = 0;
		for (Pmwx::Halfedge_iterator iter = ioSrcMap.halfedges_begin(); iter != ioSrcMap.halfedges_end(); ++iter, ++ctr)
		if (iter->mDominant)
		{
			PROGRESS_CHECK(func, 0, 2, "Merging edges into map...", ctr, total, step)

			info.srcEdge = iter;
			map<Vertex_handle, Vertex_handle>::iterator hint = vertMap.find(iter->source());
			if (hint != vertMap.end()) 
			{
				++fast;
				ioDstMap.insert_edge(iter->source()->point(), iter->target()->point(), hint->second->halfedge(), Pmwx::locate_Vertex, __MergeMaps_EdgeNotifier, &info);
			} else {
				++slow;
				ioDstMap.insert_edge(iter->source()->point(), iter->target()->point(), __MergeMaps_EdgeNotifier, &info);
			}
		}
	}

	PROGRESS_DONE(func, 0, 2, "Merging edges into map...")

	PROGRESS_START(func, 1, 2, "Copying face metadata...")

	// STEP 2 - map a faces boundary from src to dst.  Flood fill to find the corresponding faces and copy
	// what's needed.

	ctr = 0;
	total = ioSrcMap.number_of_faces();
	step = total / 200;

	for (Pmwx::Face_iterator fiter = ioSrcMap.faces_begin(); fiter != ioSrcMap.faces_end(); ++fiter, ++ctr)
	if (!fiter->is_unbounded())
	// Fast eval - if the source face is uninteresting, skip this whole loop!
	if (fiter->mAreaFeature.mFeatType != NO_VALUE || fiter->data().mTerrainType != terrain_Natural)
	{
		PROGRESS_CHECK(func, 1, 2, "Copying face metadata...", ctr, total, step)


		set<Halfedge_handle>	borders_old, borders_new;

		FindEdgesForFace(fiter, borders_old);
		
		set<Face_handle>	sanity;
		FindFacesForEdgeSet(borders_old, sanity);
		DebugAssert(sanity.size() == 1);
		DebugAssert(*sanity.begin() == &*fiter);

		DebugAssert(!borders_old.empty());

		// go through CCB - add all dests in edgemap from each CCB to borders
		for (set<Halfedge_handle>::iterator e = borders_old.begin(); e != borders_old.end(); ++e)
		{
			pair<multimap<Halfedge_handle, Halfedge_handle>::iterator, multimap<Halfedge_handle, Halfedge_handle>::iterator>	range;
			range = edgeMap.equal_range(*e);			
			DebugAssert(range.first != range.second);
			
			for (multimap<Halfedge_handle, Halfedge_handle>::iterator i = range.first; i != range.second; ++i)
			{
				borders_new.insert(i->second);
			}
		}
		DebugAssert(!borders_new.empty());

		// Next find facse for the edge set
		set<Face_handle>		faces;
		
		FindFacesForEdgeSet(borders_new, faces);		
		DebugAssert(!faces.empty());

		// Finally, mark all items in faces.
	
		for (set<Face_handle>::iterator face = faces.begin(); face != faces.end(); ++face)
		{
			if (outFaces)	outFaces->insert(*face);
			if (fiter->data().mTerrainType != terrain_Natural)	
			if (inForceProps || (*face)->data().mTerrainType == terrain_Natural)
				(*face)->data().mTerrainType = fiter->data().mTerrainType;

			if (fiter->mAreaFeature.mFeatType != NO_VALUE)
			if (inForceProps || (*face)->mAreaFeature.mFeatType == NO_VALUE)
				(*face)->mAreaFeature.mFeatType = fiter->mAreaFeature.mFeatType;

		}
	}
	PROGRESS_DONE(func, 1, 2, "Copying face metadata...")

}


// Vertex hasher - to be honest I forget why this was necessary, but without it the 
// STL couldn't build the hash table.  Foo.
#if 0 //!MSC
struct hash_vertex {
	typedef Vertex_handle KeyType;
	size_t operator()(const KeyType& key) const { return (size_t) key; }
};
#endif

// Map swap - Basically we build up mapping between the two maps and flood fill to
// find the insides.
void	SwapMaps(	Pmwx& 							ioMapA, 
					Pmwx& 							ioMapB, 
					const vector<Halfedge_handle>&	inBoundsA,
					const vector<Halfedge_handle>&	inBoundsB)
{
	DebugAssert(inBoundsA.size() == inBoundsB.size());
	
	set<Face_handle>		moveFaceFromA, moveFaceFromB;
	set<Halfedge_handle>	moveEdgeFromA, moveEdgeFromB;
	set<Vertex_handle >	moveVertFromA, moveVertFromB;
#if 1 //MSC
	hash_map<Vertex_handle, Vertex_handle>	keepVertFromA, keepVertFromB;
	hash_map<Vertex_handle, Vertex_handle>::iterator findVert;
#else
	hash_map<Vertex_handle, Vertex_handle, hash_vertex>	keepVertFromA, keepVertFromB;
	hash_map<Vertex_handle, Vertex_handle, hash_vertex>::iterator findVert;
#endif
	set<Face_handle>::iterator		faceIter;
	set<Halfedge_handle>::iterator	edgeIter;	
	set<Vertex_handle>::iterator		vertIter;	
	int n;	

#if DEV
	for (n = 0; n < inBoundsA.size(); ++n)
	{
		DebugAssert(inBoundsA[n]->target()->point() == inBoundsB[n]->target()->point());
		DebugAssert(inBoundsA[n]->source()->point() == inBoundsB[n]->source()->point());
		DebugAssert(inBoundsA[n]->face() != inBoundsA[n]->twin()->face());
		DebugAssert(inBoundsB[n]->face() != inBoundsB[n]->twin()->face());
	}
#endif

	/********************************************************************************
	 * PREPERATION - FIGURE OUT WHO IS MOVING WHERE.
	 ********************************************************************************/

	// All of the inner bounds have to move.
	moveEdgeFromA.insert(inBoundsA.begin(), inBoundsA.end());
	moveEdgeFromB.insert(inBoundsB.begin(), inBoundsB.end());

	// Inner bounds dictate contained faces, all of which move.
	FindFacesForEdgeSet(moveEdgeFromA, moveFaceFromA);
	FindFacesForEdgeSet(moveEdgeFromB, moveFaceFromB);

	// Accumulate total set of edges that must move from faces (CCBs and holes.)
	for (faceIter = moveFaceFromA.begin(); faceIter != moveFaceFromA.end(); ++faceIter)
		FindEdgesForFace(*faceIter, moveEdgeFromA);

	for (faceIter = moveFaceFromB.begin(); faceIter != moveFaceFromB.end(); ++faceIter)
		FindEdgesForFace(*faceIter, moveEdgeFromB);

	for (n = 0; n < inBoundsA.size(); ++n)
	{
		// We want stuff on the crop edge to go to the inside.  This is because 99% of the time
		// the crop inside is used.  So make sure it is dominant.  It's okay to f--- with dominance
		// because the edge gets resorted later when we do the move of the halfedge.
		if (!inBoundsA[n]->data().mDominant)		inBoundsA[n]->data().SwapDominance(inBoundsA[n].twin().data());
		if (!inBoundsB[n]->data().mDominant)		inBoundsB[n]->data().SwapDominance(inBoundsB[n].twin().data());	

		// Vertices on the CCB do NOT move since they are used by exterior stuff.
		// We need to know the correspondence for later!  So build a map
		// of how they relate for quick access.
#if 1 //MSC
		keepVertFromA.insert(hash_map<Vertex_handle, Vertex_handle>::value_type(inBoundsA[n]->target(), inBoundsB[n]->target()));
		keepVertFromB.insert(hash_map<Vertex_handle, Vertex_handle>::value_type(inBoundsB[n]->target(), inBoundsA[n]->target()));
#else
		keepVertFromA.insert(hash_map<Vertex_handle, Vertex_handle, hash_vertex>::value_type(inBoundsA[n]->target(), inBoundsB[n]->target()));
		keepVertFromB.insert(hash_map<Vertex_handle, Vertex_handle, hash_vertex>::value_type(inBoundsB[n]->target(), inBoundsA[n]->target()));
#endif
	}

	// Accume all non-saved vertices as moving.
	for (edgeIter = moveEdgeFromA.begin(); edgeIter != moveEdgeFromA.end(); ++edgeIter)
		if (keepVertFromA.count((*edgeIter)->target()) == 0)
			moveVertFromA.insert((*edgeIter)->target());

	for (edgeIter = moveEdgeFromB.begin(); edgeIter != moveEdgeFromB.end(); ++edgeIter)
		if (keepVertFromB.count((*edgeIter)->target()) == 0)
			moveVertFromB.insert((*edgeIter)->target());


	/********************************************************************************
	 * SANITY CHECK!
	 ********************************************************************************/

#if DEV
	// We don't move the unbounded face!!
	for (faceIter = moveFaceFromA.begin(); faceIter != moveFaceFromA.end(); ++faceIter)
		DebugAssert(!(*faceIter)->is_unbounded());

	for (faceIter = moveFaceFromB.begin(); faceIter != moveFaceFromB.end(); ++faceIter)
		DebugAssert(!(*faceIter)->is_unbounded());

	// Edges all ref a face that's moving too
	for (edgeIter = moveEdgeFromA.begin(); edgeIter != moveEdgeFromA.end(); ++edgeIter)
	{
		DebugAssert(!(*edgeIter)->face()->is_unbounded());
		DebugAssert(moveFaceFromA.count((*edgeIter)->face()) > 0);
	}

	for (edgeIter = moveEdgeFromB.begin(); edgeIter != moveEdgeFromB.end(); ++edgeIter)
	{
		DebugAssert(!(*edgeIter)->face()->is_unbounded());
		DebugAssert(moveFaceFromB.count((*edgeIter)->face()) > 0);
	}
	// TODO: confirm all bounds of all faces are in the edge set?
#endif



	/********************************************************************************
	 * FIX REFS TO NON-MOVING VERTICES
	 ********************************************************************************/

	// For each vertex we are keeping, if it references a CCB halfedge, better swap it
	// over to the new CCB.	 We take the cheap way out and always force the vertex ptr!
	for (n = 0; n < inBoundsA.size(); ++n)
	{
		inBoundsA[n]->target()->set_halfedge(inBoundsB[n]);
		inBoundsB[n]->target()->set_halfedge(inBoundsA[n]);
	}

	 // For each halfedge we are moving, we need to see if it references a non-moving
	 // vertex.  If so, we need to make sure it now references the new one.
	for (edgeIter = moveEdgeFromA.begin(); edgeIter != moveEdgeFromA.end(); ++edgeIter)
	{
		findVert = keepVertFromA.find((*edgeIter)->target());
		if (findVert != keepVertFromA.end())
			(*edgeIter)->set_target(findVert->second);
	}

	for (edgeIter = moveEdgeFromB.begin(); edgeIter != moveEdgeFromB.end(); ++edgeIter)
	{
		findVert = keepVertFromB.find((*edgeIter)->target());
		if (findVert != keepVertFromB.end())
			(*edgeIter)->set_target(findVert->second);
	}

	/********************************************************************************
	 * SWAP THE CCB
	 ********************************************************************************/

	// Wait this is really easy!  Faces aren't moving.  The "next" of each ring is
	// preserved.  We've already adjusted all vertices.  So twin swapping is all
	// it takes!

	for (n = 0; n < inBoundsA.size(); ++n)
	{
		Halfedge_handle a_twin = inBoundsA[n]->twin();
		Halfedge_handle b_twin = inBoundsB[n]->twin();

		a_twin->set_twin(inBoundsB[n]);
		b_twin->set_twin(inBoundsA[n]);

		inBoundsA[n]->set_twin(b_twin);
		inBoundsB[n]->set_twin(a_twin);

		// Make sure dominance is on inside.
		DebugAssert(inBoundsA[n]->mDominant);
		DebugAssert(inBoundsB[n]->mDominant);
	}


	/********************************************************************************
	 * MIGRATE ALL ENTITIES
	 ********************************************************************************/

	// Finally we just need to swap each entity into a new list.

	for (faceIter = moveFaceFromA.begin(); faceIter != moveFaceFromA.end(); ++faceIter)
		ioMapB.MoveFaceToMe(&ioMapA, *faceIter);
	for (faceIter = moveFaceFromB.begin(); faceIter != moveFaceFromB.end(); ++faceIter)
		ioMapA.MoveFaceToMe(&ioMapB, *faceIter);

	for (vertIter = moveVertFromA.begin(); vertIter != moveVertFromA.end(); ++vertIter)
		ioMapA.UnindexVertex(*vertIter);
	for (vertIter = moveVertFromB.begin(); vertIter != moveVertFromB.end(); ++vertIter)
		ioMapB.UnindexVertex(*vertIter);
	for (vertIter = moveVertFromA.begin(); vertIter != moveVertFromA.end(); ++vertIter)
		ioMapB.MoveVertexToMe(&ioMapA, *vertIter);
	for (vertIter = moveVertFromB.begin(); vertIter != moveVertFromB.end(); ++vertIter)
		ioMapA.MoveVertexToMe(&ioMapB, *vertIter);
	for (vertIter = moveVertFromA.begin(); vertIter != moveVertFromA.end(); ++vertIter)
		ioMapB.ReindexVertex(*vertIter);
	for (vertIter = moveVertFromB.begin(); vertIter != moveVertFromB.end(); ++vertIter)
		ioMapA.ReindexVertex(*vertIter);

	for (int n = 0; n < inBoundsA.size(); ++n)
	{
		ioMapB.MoveHalfedgeToMe(&ioMapA, inBoundsA[n]);
		ioMapA.MoveHalfedgeToMe(&ioMapB, inBoundsB[n]);
		moveEdgeFromA.erase(inBoundsA[n]);
		moveEdgeFromB.erase(inBoundsB[n]);
	}

	for (edgeIter = moveEdgeFromA.begin(); edgeIter != moveEdgeFromA.end(); ++edgeIter)
	if ((*edgeIter)->mDominant)
		ioMapB.MoveEdgeToMe(&ioMapA, *edgeIter);
	for (edgeIter = moveEdgeFromB.begin(); edgeIter != moveEdgeFromB.end(); ++edgeIter)
	if ((*edgeIter)->mDominant)
		ioMapA.MoveEdgeToMe(&ioMapB, *edgeIter);

}

#endif

#if 0
// Sort op to sort a bbox bby its lower bounds.
struct	__sort_by_bbox_base {
	bool operator()(const Bbox_2& lhs, const Bbox_2& rhs) const { return lhs.y()min() < rhs.y()min(); }
};

// I think we didn't end up needing this...the idea was to avoid tiny slivers.
static bool __epsi_intersect(const Segment_2& segA, const Segment_2& segB, double epsi, Point_2& cross)
{
	if (segA.is_horizontal() || segA.is_vertical() ||
		segB.is_horizontal() || segB.is_vertical())
	{
		return segA.intersect(segB, cross);
	}

	if (!Line2(segA).intersect(Line2(segB), cross)) return false;

//	epsi = epsi * epsi;
	
	double	da1 = Vector2(segA.source(), cross).squared_length();
	double	da2 = Vector2(segA.target(), cross).squared_length();
	double	db1 = Vector2(segB.source(), cross).squared_length();
	double	db2 = Vector2(segB.target(), cross).squared_length();
	
	if (da1 < epsi || da2 < epsi || db1 < epsi || db2 < epsi)
	{
		double da = (da1 < da2) ? da1 : da2;
		Point_2 pa = (da1 < da2) ? segA.source() : segA.target();
		double db = (db1 < db2) ? db1 : db2;
		Point_2 pb = (db1 < db2) ? segB.source() : segB.target();
		
		if (da < epsi && db < epsi)
		{
			cross = (da < db) ? pa : pb;
			return true;
		} else if (da < epsi) {
			cross = pa;
			return true;
		} else {
			DebugAssert(db < epsi);
			cross = pb;
			return true;
		}
	}
	return segA.collinear_has_on(cross) && segB.collinear_has_on(cross);
}
#endif

#if 0
/*
 * TopoIntegrateMaps
 *
 * The idea here is pretty simple: we are going to insert points into both A and B so that anywhere edges cross
 * in A and B there is a pre-inserted vertex, and more importantly that vertex has the EXACT same value in both
 * pmwxs.  That means that when we go to merge, there will not be any line-line intersections and the resulting
 * map will not have slivering problems and will be topologically correct.
 *
 * Example: point P of an overlay map is almost colinear with halfedge H in the main map.  But P has two halfedges
 * Pa and Pb on opposite sides of H.  When we insert Pa and Pb, we will get a judgement that each segment may or may
 * not cross H (and if it does) at a crossing point Ca and Cb that will be different, because the intersection algorithm
 * "jitters" based on the slope of Pa and Pb which are different.  In the worst case, P is taken to be on the same side
 * of H as Pa and Pb, resulting in topologically different locations for P and a double-point insertion.
 *
 * By pre-breaking Pa and/or Pb at a single crossing point C that is then inserted into H, we guarantee consistent
 * topological judgements for P (it must be on exactly one side of C by the topological relationships of the overlay
 * map since the overlay is modified to have C inserted as a split edge in only one of Pa ir Pb) and we don't get a
 * double insert of P.
 *
 */

 // Colinear check - this tells us if two points are very close to each other but not truly colinear.
#if 0
#define	SMALL_SEG_CUTOFF	0.01
#define BBOX_SLOP			0.00001
#define NEAR_SLIVER_COLINEAR 7.7e-12
#else
#define	SMALL_SEG_CUTOFF	0.005
#define BBOX_SLOP			0.00001
#define NEAR_SLIVER_COLINEAR 7.7e-12
#endif

inline bool	__near_colinear(const Segment_2& seg, Point_2& p)
{
	// Don't return true for the degenerate case - isn't useful!
	if (seg.source() == p) return false;
	if (seg.target() == p) return false;
	// Special case h and v.
	if (seg.is_horizontal())
		return p.y() == seg.source().y() && ((seg.source().x() < p.x() && p.x() < seg.target().x()) || (seg.target().x() < p.x() && p.x() < seg.source().x()));
	if (seg.is_vertical())
		return p.x() == seg.source().x() && ((seg.source().y() < p.y() && p.y() < seg.target().y()) || (seg.target().y() < p.y() && p.y() < seg.source().y()));
	
	// OK, now we need to switch to CGAL's kernel
	
	CGAL::Point_2<FastKernel> p1 = CGAL::Point_2<FastKernel>(p.x(), p.y());
	CGAL::Segment_2<FastKernel> seg1 = CGAL::Segment_2<FastKernel>(
																   CGAL::Point_2<FastKernel>(seg.source().x(), seg.source().y()),
																   CGAL::Point_2<FastKernel>(seg.target().x(), seg.target().y()));
	CGAL::Point_2<FastKernel>	pp = seg1.supporting_line().projection(p1);
	
	if (p1 == pp || CGAL::Vector_2<FastKernel>(p1, pp).squared_length() < NEAR_SLIVER_COLINEAR)
	{
		// BEN SAYS: 11/11/07 - new check.  This is the non-horizontal/vertical colinear case.  Basically we're trying to divert segments
		// slightly to avoid slivers.  Problem is, the dot product we use below to make sure we are within the SPAN of the segment is not very
		// accurate and tends to let points through that are NOT really "within" the span of the segment, causing problems.
		//
		// This isn't a fix, but by rejecting these points we do slightly improve quality - quick BBox test.  NO WAY a point is outside the seg's
		// bbox and still "near-colinear-on".  
		if (p.x() <= seg.source().x() || p.x() >= seg.target().x() || p.y() <= seg.source().y() || p.y() >= seg.target().y()) return false;
			
		return (CGAL::Vector_2<FastKernel>(seg1.source(), seg1.target()) * (CGAL::Vector_2<FastKernel>(seg1.source(), p1)) > 0.0) &&
			   (CGAL::Vector_2<FastKernel>(seg1.target(), seg1.source()) * (CGAL::Vector_2<FastKernel>(seg1.target(), p1)) > 0.0);
	}
	return false;
}


// Topo integration.  Basically for any two colinear segments, insert each other's
// end poitns into the map, and for any crossing segments, insert the crossing point.
// The hope is that:
// (1) there will be no crossings between segs in map A and B - only segments ending
// in common vertices (who are perfectly equal), and
// (2) where segments overlap, they are broken into two perfectly overlapping segments
// and non-overlapping segments.
//
// Please note that where a point P is near-colinear to an edge E, that edge E ends up
// CHANGING to E1 and E2 to meet point P.  Basically we'd rather ruin our map than get
// a sliver.
//
// We spatially index the bounding box of all edges to speed up intersection finding!
// Also please note that we split all indexed boxe sinto "big and small" by some
// arbitrary size...the idea is: we have to look at big items a lot so we put the small
// pieces in a separate container that can be searched with less "slop".
//
// WARNING: SMALL_SEG_CUTOFF is hard-coded!
void TopoIntegrateMaps(Pmwx * mapA, Pmwx * mapB)
{
// OPTIMIZE: to be explored - would a merge sort (after sorting both sets) be time-faster
// than what we have now?  How well does the double-partition work?

	typedef multimap<Bbox_2, Halfedge_handle, __sort_by_bbox_base>		HalfedgeMap;
	typedef multimap<Halfedge_handle, Point_2>		SplitMap;
	typedef	set<Halfedge_handle>					SplitSet;
	typedef map<double, Point_2>					SortedPoints;
		
		HalfedgeMap				map_small;
		HalfedgeMap				map_big;

		double					yrange_small = 0.0;
		double					yrange_big = 0.0;

		SplitMap				splitA, splitB;
		SplitSet				setA, setB;
		Pmwx::Halfedge_iterator iterA;
		Pmwx::Halfedge_iterator iterB;
		Point_2					p;
		double					dist;
		pair<HalfedgeMap::iterator, HalfedgeMap::iterator>	possibles;
		HalfedgeMap::iterator								he_box;

		pair<SplitMap::iterator, SplitMap::iterator>		range;
		SplitSet::iterator 									he;
		SplitMap::iterator									splitIter;
		SortedPoints::iterator								spi;

		SortedPoints 										splits;
		Halfedge_handle										subdiv;
		Bbox_2												key_lo, key_hi;

	// Note: we want to index the larger pmwx.  This way we can rule out by spatial sorting a lot more checks.
	if (mapA->number_of_halfedges() > mapB->number_of_halfedges())
	{
#if TRACE_TOPO_INTEGRATE
		printf("NOT Swapping maps for speed.\n");
#endif
	
		//swap(mapA, mapB);
	}

	// We are going to store the halfedges of B in two maps, sorted by their bbox's ymin.
	// The idea here is to have a set of very small segments...we can be real precise in the
	// range we scan here.  The big map will have all the huge exceptions, which we'll have to
	// totally iterate, but that wil be a much smaller set.
	for (iterB = mapB->halfedges_begin(); iterB != mapB->halfedges_end(); ++iterB)
	if (iterB->mDominant)
	{
		Bbox_2	bounds(iterB->source()->point(),iterB->target()->point());
		bounds.expand(BBOX_SLOP);
		
		dist = bounds.y()max() - bounds.y()min();
		if (dist > SMALL_SEG_CUTOFF)
		{
			yrange_big = max(yrange_big, dist);
			map_big.insert(HalfedgeMap::value_type(bounds, iterB));
		} else {
			yrange_small = max(yrange_small, dist);
			map_small.insert(HalfedgeMap::value_type(bounds, iterB));
		}

	}

	// Go through and search both the small and big set, looking for halfedges.
	for (iterA = mapA->halfedges_begin(); iterA != mapA->halfedges_end(); ++iterA)
	if (iterA->mDominant)
	{
		Segment_2	segA(iterA->source()->point(), iterA->target()->point());
		Bbox_2		boxA(iterA->source()->point(), iterA->target()->point());
		boxA.expand(BBOX_SLOP);

		bool did_colinear;
		key_lo = Point_2(0.0, boxA.y()min() - yrange_big);
		key_hi = Point_2(0.0, boxA.y()max());
		possibles.first = map_big.lower_bound(key_lo);
		possibles.second = map_big.upper_bound(key_hi);
		for (he_box = possibles.first; he_box != possibles.second; ++he_box)
		{
			if (boxA.overlap(he_box->first))
			{
				Segment_2	segB(he_box->second->source()->point(), he_box->second->target()->point());
				
				// IMPORTANT: we need to check for the almost-colinear case first.  Why?  Well, it turns out that
				// in a near-colinear case, sometimes the intersect route will return an intersection.  But we do
				// not want to insert some bogus point P into two lines if really they overlap!  P might be epsi-
				// equal to an end point and then we're f--ed.  So do colinear first and skip the intersection
				// if we find one.

				did_colinear = false;
				if (segA.is_horizontal() == segB.is_horizontal() && segA.is_vertical() == segB.is_vertical())
				{
					if (__near_colinear(segA, segB.source()))
					{
						did_colinear = true;
						splitA.insert(SplitMap::value_type(iterA, segB.source()));
						setA.insert(iterA);
#if TRACE_TOPO_INTEGRATE
						printf("Will split A %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y(),segB.source().x(),segB.source().y());
#endif						
					}
					if (__near_colinear(segA, segB.target()))
					{
						did_colinear = true;
						splitA.insert(SplitMap::value_type(iterA, segB.target()));
						setA.insert(iterA);
#if TRACE_TOPO_INTEGRATE
						printf("Will split A %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y(),segB.target().x(),segB.target().y());
#endif						
					}
					if (__near_colinear(segB, segA.source()))
					{
						did_colinear = true;
						splitB.insert(SplitMap::value_type(he_box->second, segA.source()));
						setB.insert(he_box->second);
#if TRACE_TOPO_INTEGRATE
						printf("Will split B %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y(),segA.source().x(),segA.source().y());
#endif						
					}
					if (__near_colinear(segB, segA.target()))
					{
						did_colinear = true;
						splitB.insert(SplitMap::value_type(he_box->second, segA.target()));
						setB.insert(he_box->second);
#if TRACE_TOPO_INTEGRATE
						printf("Will split B %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y(),segA.target().x(),segA.target().y());
#endif						
					}
				}
				
				//				if (!did_colinear && segA.intersect(segB, p))	//__epsi_intersect(segA, segB, NEAR_INTERSECT_DIST, p))
				if (!did_colinear) 
				{
					CGAL::Object iresult = CGAL::intersection(CGAL::Segment_2<FastKernel>(
																						  CGAL::Point_2<FastKernel>(segA.source().x(), segA.source().y()),
																						  CGAL::Point_2<FastKernel>(segA.target().x(), segA.target().y())),
															  CGAL::Segment_2<FastKernel>(
																						  CGAL::Point_2<FastKernel>(segB.source().x(), segB.source().y()),
																						  CGAL::Point_2<FastKernel>(segB.target().x(), segB.target().y()))
															  );
					CGAL::Point_2<FastKernel> ipoint;
					if (CGAL::assign(ipoint, iresult)) // intersection is a point
					{
						p = Point_2( CGAL::to_double(ipoint.x()),  CGAL::to_double(ipoint.y()));
						if (ipoint != CGAL::Point_2<FastKernel>(segA.source().x(), segA.source().y()) && ipoint != CGAL::Point_2<FastKernel>(segA.target().x(), segA.target().y()))
					{
						splitA.insert(SplitMap::value_type(iterA, p));
						setA.insert(iterA);
#if TRACE_TOPO_INTEGRATE
						printf("Will split A %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: intersect from %.15lf,%.15lf->%.15lf,%.15lf.\n",
									segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y(),  p.x(),p.y(),  segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y());
#endif						
						
					}
						if (ipoint != CGAL::Point_2<FastKernel>(segB.source().x(), segB.source().y()) && ipoint != CGAL::Point_2<FastKernel>(segB.target().x(), segB.target().y()))
					{
						splitB.insert(SplitMap::value_type(he_box->second, p));
						setB.insert(he_box->second);
#if TRACE_TOPO_INTEGRATE
						printf("Will split B %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: intersect from %.15lf,%.15lf->%.15lf,%.15lf.\n",
									segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y(),  p.x(),p.y(),  segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y());
#endif						
					}
				}
			}
		}
		}
		
		key_lo = Point_2(0.0, boxA.y()min() - yrange_small);
		key_hi = Point_2(0.0, boxA.y()max());
		possibles.first = map_small.lower_bound(key_lo);
		possibles.second = map_small.upper_bound(key_hi);
		for (he_box = possibles.first; he_box != possibles.second; ++he_box)
		{
			if (boxA.overlap(he_box->first))
			{
				Segment_2	segB(he_box->second->source()->point(), he_box->second->target()->point());
				did_colinear = false;				
				
				if (segA.is_horizontal() == segB.is_horizontal() && segA.is_vertical() == segB.is_vertical())
				{
					if (__near_colinear(segA, segB.source()))
					{
						did_colinear = true;				
						splitA.insert(SplitMap::value_type(iterA, segB.source()));
						setA.insert(iterA);
#if TRACE_TOPO_INTEGRATE
						printf("Will split A %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y(),segB.source().x(),segB.source().y());
#endif						
					}
					if (__near_colinear(segA, segB.target()))
					{
						did_colinear = true;				
						splitA.insert(SplitMap::value_type(iterA, segB.target()));
						setA.insert(iterA);
#if TRACE_TOPO_INTEGRATE
						printf("Will split A %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y(),segB.target().x(),segB.target().y());
#endif						
					}
					if (__near_colinear(segB, segA.source()))
					{
						did_colinear = true;				
						splitB.insert(SplitMap::value_type(he_box->second, segA.source()));
						setB.insert(he_box->second);
#if TRACE_TOPO_INTEGRATE
						printf("Will split B %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y(),segA.source().x(),segA.source().y());
#endif						
					}
					if (__near_colinear(segB, segA.target()))
					{
						did_colinear = true;				
						splitB.insert(SplitMap::value_type(he_box->second, segA.target()));
						setB.insert(he_box->second);
#if TRACE_TOPO_INTEGRATE
						printf("Will split B %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: near collinear.\n",
								segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y(),segA.target().x(),segA.target().y());
#endif						
					}
				}
				
				//if (!did_colinear && segA.intersect(segB, p))
					//				if (!did_colinear && segA.intersect(segB, p))	//__epsi_intersect(segA, segB, NEAR_INTERSECT_DIST, p))
				if (!did_colinear) 
				{
					CGAL::Object iresult = CGAL::intersection(CGAL::Segment_2<FastKernel>(
																						  CGAL::Point_2<FastKernel>(segA.source().x(), segA.source().y()),
																						  CGAL::Point_2<FastKernel>(segA.target().x(), segA.target().y())),
															  CGAL::Segment_2<FastKernel>(
																						  CGAL::Point_2<FastKernel>(segB.source().x(), segB.source().y()),
																						  CGAL::Point_2<FastKernel>(segB.target().x(), segB.target().y()))
															  );
					CGAL::Point_2<FastKernel> ipoint;
					if (CGAL::assign(ipoint, iresult)) // intersection is a point
					{
						p = Point_2( CGAL::to_double(ipoint.x()),  CGAL::to_double(ipoint.y()));
						{
							if (ipoint != CGAL::Point_2<FastKernel>(segA.source().x(), segA.source().y()) && ipoint != CGAL::Point_2<FastKernel>(segA.target().x(), segA.target().y()))
					{
						splitA.insert(SplitMap::value_type(iterA, p));
						setA.insert(iterA);
#if TRACE_TOPO_INTEGRATE
						printf("Will split A %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: intersect from %.15lf,%.15lf->%.15lf,%.15lf.\n",
									   segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y(),  p.x(),p.y(),  segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y());
#endif						
					}
							if (ipoint != CGAL::Point_2<FastKernel>(segB.source().x(), segB.source().y()) && ipoint != CGAL::Point_2<FastKernel>(segB.target().x(), segB.target().y()))
					{
						splitB.insert(SplitMap::value_type(he_box->second, p));
						setB.insert(he_box->second);
#if TRACE_TOPO_INTEGRATE
						printf("Will split B %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf: intersect from %.15lf,%.15lf->%.15lf,%.15lf.\n",
									   segB.source().x(),segB.source().y(),segB.target().x(),segB.target().y(),  p.x(),p.y(),  segA.source().x(),segA.source().y(),segA.target().x(),segA.target().y());
#endif						
					}
				}
			}
		}
	}
		}
	}

	// Now go through and actually make the splits.
	for (he = setA.begin(); he != setA.end(); ++he)
	{
		range = splitA.equal_range(*he);
		splits.clear();

		Segment_2 origSegA((*he)->source()->point(), (*he)->target()->point());
		for (splitIter = range.first; splitIter != range.second; ++splitIter)
		{
			dist = Segment_2((*he)->source()->point(), splitIter->second).squared_length();
			splits.insert(SortedPoints::value_type(dist, splitIter->second));
		}

		subdiv = *he;
		for (spi = splits.begin(); spi != splits.end(); ++spi)
		{
#if DEV
			if (origSegA.is_vertical())
				DebugAssert(origSegA.source().x() == spi->second.x());
			if (origSegA.is_horizontal())
				DebugAssert(origSegA.source().y() == spi->second.y());
#endif
#if TRACE_TOPO_INTEGRATE
			printf("Splitting A: %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf\n", 
				subdiv->source()->point().x(),subdiv->source()->point().y(),
				subdiv->target()->point().x(),subdiv->target()->point().y(),
				spi->second.x(), spi->second.y());
#endif
		// BEN SAYS: HOLY CRAP!!! SCARY LAST MINUTE CHANGE!  Try the Dr it hurts strategy when a bad intersect gives us a vertex we already have.  Hope that in
		// the actual render the fact that the locate WILL succeed will keep things from falling appart.
		//if (mapA->locate_vertex(spi->second) == NULL)
			{
				mapA->split_edge(subdiv, spi->second);
				subdiv = subdiv->next();
			}
		}
	}

	for (he = setB.begin(); he != setB.end(); ++he)
	{
		range = splitB.equal_range(*he);
		splits.clear();

		Segment_2	origSegB((*he)->source()->point(), (*he)->target()->point());
		for (splitIter = range.first; splitIter != range.second; ++splitIter)
		{
			dist = Segment_2((*he)->source()->point(), splitIter->second).squared_length();
			splits.insert(SortedPoints::value_type(dist, splitIter->second));
		}

		subdiv = *he;
		for (spi = splits.begin(); spi != splits.end(); ++spi)
		{
#if DEV
			if (origSegB.is_vertical())
				DebugAssert(origSegB.source().x() == spi->second.x());
			if (origSegB.is_horizontal())
				DebugAssert(origSegB.source().y() == spi->second.y());
#endif				
#if TRACE_TOPO_INTEGRATE
			printf("Splitting B: %.15lf,%.15lf->%.15lf,%.15lf at %.15lf,%.15lf\n", 
				subdiv->source()->point().x(),subdiv->source()->point().y(),
				subdiv->target()->point().x(),subdiv->target()->point().y(),
				spi->second.x(), spi->second.y());
#endif
			if (mapB->locate_vertex(spi->second) == NULL)
			{
				// we may somehow have a zero length edge; refrain from trying to split it
				if (!((subdiv->source()->point().x() == subdiv->target()->point().x()) 
					  and (subdiv->source()->point().y() == subdiv->target()->point().y())))
				mapB->split_edge(subdiv, spi->second);
				subdiv = subdiv->next();
			}
		}
	}
}
#endif

#if 0
class	collect_he : public CGAL::Arr_observer<Arrangement_2> {
public:
	Halfedge_handle he;
	
  virtual void after_create_edge (Halfedge_handle e)
  {
	he = e;
	}
};

// Safe ring insert - basically check to see if the vertices exist and do the right thing.
Face_handle SafeInsertRing(Pmwx * inPmwx, Face_handle parent, const vector<Point2>& inPoints)
{
// OPTIMIZE: re-examine this now that we have vertex indexing...can we use the built in index?
// SPECULATION: even with point indexing, insert_ring is STILL faster - insert_edge, even when
// both points are known still must find the sorting point around the vertices and worse yet
// potentially has to split faces and move holes...all slow!  The ring does NONE of this.
	
	collect_he	c;
	c.attach(*inPmwx);

	for (int n = 0; n < inPoints.size(); ++n)
	{
		if(inPoints[n] != inPoints[(n+1)%inPoints.size()])
			CGAL::insert_curve(*inPmwx, Curve_2(Segment_2(Point_2(ben2cgal(inPoints[n])), Point_2(ben2cgal(inPoints[(n+1)%inPoints.size()])))));
	}
	return c.he->face();
}
#endif


/************************************************************************************************
 * MAP ANALYSIS AND RASTERIZATION/ANALYSIS
 ************************************************************************************************/
#pragma mark -

// Dominance: for conveninence I mark half my edges with a flag...this is a rapid way to
// guarantee we never list an edge and its twin even when the edge set we get is not an in-order
// iteration.  Cheesy but it works.
bool	ValidateMapDominance(const Pmwx& inMap)
{
	return true;
/*
	int doubles = 0;
	int zeros = 0;
	int ctr = 0;
	int wrong = 0;
	for (Pmwx::Halfedge_const_iterator i = inMap.halfedges_begin(); i != inMap.halfedges_end(); ++i, ++ctr)
	{
		bool ideal_dom = ((ctr % 2) == 0);
		if (i->data().mDominant != ideal_dom)
		{
#if DEV
			++wrong;
#else
			return false;
#endif
		}
		if (i->data().mDominant && i->twin()->data().mDominant)
		{
#if DEV
			++doubles;
//			printf("Double on edge: %lf,%lf -> %lf,%lf\n", 
//				CGAL::to_double(i->source()->point().x()),CGAL::to_double(i->source()->point().y()),
//				CGAL::to_double(i->target()->point().x()),CGAL::to_double(i->target()->point().y()));
#else
			return false;
#endif
		}
		if (!i->data().mDominant && !i->twin()->data().mDominant)
		{
#if DEV
			++zeros;
//			printf("Zero on edge: %lf,%lf -> %lf,%lf\n", 
//				CGAL::to_double(i->source()->point().x()),CGAL::to_double(i->source()->point().y()),
//				CGAL::to_double(i->target()->point().x()),CGAL::to_double(i->target()->point().y()));
#else
			return false;
#endif
		}
	}
#if DEV
	if (doubles > 0 || zeros > 0 || wrong > 0)
		printf("Validation : %d double-dominant halfedges, and %d zero-dominant halfedges.  %d wrong\n", doubles, zeros, wrong);
#endif
	return doubles == 0 && zeros == 0 && wrong == 0;
*/
#if !DEV
	#error rip this out like asbestos!
#endif
}






void	CalcBoundingBox(
			const Pmwx&		inMap,
			Point_2&			sw,
			Point_2&			ne)
{
	bool		inited = false;
	Bbox2		box;
	
	Face_const_handle	uf = inMap.unbounded_face();
	
	for (Pmwx::Hole_const_iterator holes = uf->holes_begin(); holes != uf->holes_end(); ++holes)
	{
		Pmwx::Ccb_halfedge_const_circulator	cur, last;
		cur = last = *holes;
		do {

			if (!inited)
			{
				box = Bbox2(cgal2ben(cur->source()->point()));
				inited = true;
			}
			
			box += cgal2ben(cur->source()->point());
			box += cgal2ben(cur->target()->point());
		
			++cur;
		} while (cur != last);
	}
	sw = ben2cgal(box.p1);
	ne = ben2cgal(box.p2);
}			


double	GetMapFaceAreaMeters(const Face_handle f)
{
	if (f->is_unbounded()) return -1.0;
	Polygon2	outer;
	Pmwx::Ccb_halfedge_circulator	circ = f->outer_ccb();
	Pmwx::Ccb_halfedge_circulator	start = circ;
	do {
			outer.push_back(cgal2ben(circ->source()->point()));
		++circ;
	} while (circ != start);
	
	CoordTranslator2 trans;
	
	CreateTranslatorForPolygon(outer, trans);

	for (int n = 0; n < outer.size(); ++n)
	{
		outer[n] = trans.Forward(outer[n]);
	}

	double me = outer.area();

	for (Pmwx::Hole_iterator h = f->holes_begin(); h != f->holes_end(); ++h)
	{
		Polygon2	ib;
		Pmwx::Ccb_halfedge_circulator	circ(*h);
		Pmwx::Ccb_halfedge_circulator	start = circ;
		do {
				ib.push_back(Point2(CGAL::to_double(circ->source()->point().x()),CGAL::to_double(circ->source()->point().y())));
			++circ;
		} while (circ != start);

		for (int n = 0; n < ib.size(); ++n)
			ib[n] = trans.Forward(ib[n]);

		me += ib.area();
	}
	return me;
}

double	GetMapEdgeLengthMeters(const Pmwx::Halfedge_handle e)
{
	return LonLatDistMeters(
				CGAL::to_double(e->source()->point().x()),
				CGAL::to_double(e->source()->point().y()),
				CGAL::to_double(e->target()->point().x()),
				CGAL::to_double(e->target()->point().y()));
}

float	GetParamAverage(const Pmwx::Face_handle f, const DEMGeo& dem, float * outMin, float * outMax)
{
	PolyRasterizer	rast;
	int count = 0;
	float e;
	float avg = 0.0;
	int y = SetupRasterizerForDEM(f, dem, rast);
	rast.StartScanline(y);
	while (!rast.DoneScan())
	{
		int x1, x2;
		while (rast.GetRange(x1, x2))
		{
			for (int x = x1; x < x2; ++x)
			{
				e = dem.get(x,y);
				if (e != DEM_NO_DATA)
				{
					if (count == 0)
					{
						if (outMin) *outMin = e;
						if (outMax) *outMax = e;
					} else {
						if (outMin) *outMin = min(*outMin,e);
						if (outMax) *outMax = max(*outMax,e);
					}
					avg += e;
					count++;
				}
			}
		}
		++y;
		if (y >= dem.mHeight) break;
		rast.AdvanceScanline(y);
	}
	if (count == 0)
	{
		Pmwx::Ccb_halfedge_const_circulator stop, i;
		i = stop = f->outer_ccb();
		do {
			e = dem.value_linear(CGAL::to_double(i->source()->point().x()),CGAL::to_double(i->source()->point().y()));
			if (e != DEM_NO_DATA)
			{
				if (count == 0)
				{
					if (outMin) *outMin = e;
					if (outMax) *outMax = e;
				} else {
					if (outMin) *outMin = min(*outMin,e);
					if (outMax) *outMax = max(*outMax,e);
				}
				avg += e;
				count++;
			}
			++i;
		} while (i != stop);
	}
	if (count > 0)
		return avg / (float) count;
	else
		return DEM_NO_DATA;
}

int	GetParamHistogram(const Pmwx::Face_handle f, const DEMGeo& dem, map<float, int>& outHistogram)
{
	PolyRasterizer	rast;
	int count = 0;
	int e;
	int y = SetupRasterizerForDEM(f, dem, rast);
	rast.StartScanline(y);
	while (!rast.DoneScan())
	{
		int x1, x2;
		while (rast.GetRange(x1, x2))
		{
			for (int x = x1; x < x2; ++x)
			{
				e = dem.get(x,y);
				if (e != DEM_NO_DATA)
					count++, outHistogram[e]++;
			}
		}
		++y;
		if (y >= dem.mHeight) break;
		rast.AdvanceScanline(y);
	}
	if (count == 0)
	{
		Pmwx::Ccb_halfedge_const_circulator stop, i;
		i = stop = f->outer_ccb();
		do {
			e = dem.xy_nearest(CGAL::to_double(i->source()->point().x()),CGAL::to_double(i->source()->point().y()));
			if (e != DEM_NO_DATA)
				count++, outHistogram[e]++;
			++i;
		} while (i != stop);
	}
	return count;
}

bool	ClipDEMToFaceSet(const set<Face_handle>& inFaces, const DEMGeo& inSrcDEM, DEMGeo& inDstDEM, int& outX1, int& outY1, int& outX2, int& outY2)
{
	set<Halfedge_handle>		allEdges, localEdges;
	for (set<Face_handle>::const_iterator f = inFaces.begin(); f != inFaces.end(); ++f)
	{
		localEdges.clear();
		FindEdgesForFace(*f, localEdges);
		for (set<Halfedge_handle>::iterator e = localEdges.begin(); e != localEdges.end(); ++e)
		{
			if (inFaces.count((*e)->face()) == 0 || inFaces.count((*e)->twin()->face()) == 0)
				allEdges.insert(*e);
		}
	}

	PolyRasterizer	rast;
	int x, y;
	y = SetupRasterizerForDEM(allEdges, inSrcDEM, rast);

	outX1 = inSrcDEM.mWidth;
	outX2 = 0;
	outY1 = inSrcDEM.mHeight;
	outY2 = 0;
	bool ok = false;

	rast.StartScanline(y);
	while (!rast.DoneScan())
	{
		int x1, x2;
		while (rast.GetRange(x1, x2))
		{
			for (x = x1; x < x2; ++x)
			{
				outX1 = min(outX1, x);
				outX2 = max(outX2, x+1);
				outY1 = min(outY1, y);
				outY2 = max(outY2, y+1);
				ok = true;

				if (inSrcDEM.get(x,y) != DEM_NO_DATA)
					inDstDEM(x,y) = inSrcDEM(x,y);
			}
		}
		++y;
		if (y >= inSrcDEM.mHeight) break;
		rast.AdvanceScanline(y);
	}
	if (outX1 < 0)	outX1 = 0;
	if (outY1 < 0)	outY1 = 0;
	if (outX2 > inDstDEM.mWidth) outX2 = inDstDEM.mWidth;
	if (outY2 > inDstDEM.mHeight) outY2 = inDstDEM.mHeight;
	return ok;
}

int		SetupRasterizerForDEM(const Pmwx::Face_handle f, const DEMGeo& dem, PolyRasterizer& rasterizer)
{
	set<Halfedge_handle>	all, useful;
	FindEdgesForFace(f, all);
	for (set<Halfedge_handle>::iterator e = all.begin(); e != all.end(); ++e)
	{
		if ((*e)->face() != (*e)->twin()->face())
			useful.insert(*e);
	}
	return SetupRasterizerForDEM(useful, dem, rasterizer);
}

int		SetupRasterizerForDEM(const set<Halfedge_handle>& inEdges, const DEMGeo& dem, PolyRasterizer& rasterizer)
{
	for (set<Halfedge_handle>::const_iterator e = inEdges.begin(); e != inEdges.end(); ++e)
	{
		double x1 = dem.lon_to_x(CGAL::to_double((*e)->source()->point().x()));
		double y1 = dem.lat_to_y(CGAL::to_double((*e)->source()->point().y()));
		double x2 = dem.lon_to_x(CGAL::to_double((*e)->target()->point().x()));
		double y2 = dem.lat_to_y(CGAL::to_double((*e)->target()->point().y()));

		if (y1 != y2)
		{
			if (y1 < y2)
				rasterizer.masters.push_back(PolyRasterSeg_t(x1,y1,x2,y2));
			else
				rasterizer.masters.push_back(PolyRasterSeg_t(x2,y2,x1,y1));
		}
	}

	rasterizer.SortMasters();

	if (rasterizer.masters.empty())
		 return 0;
	return floor(rasterizer.masters.front().y1);
}

