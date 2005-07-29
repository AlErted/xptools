#include "GISTool_VectorCmds.h"
#include "GISTool_Utils.h"
#include "GISTool_Globals.h"
#include "PerfUtils.h"
#include "AssertUtils.h"
#include <ShapeFil.h>
#include "MapAlgs.h"
#include "SDTSReadTVP.h"
#include "GISUtils.h"
#include "ParamDefs.h"
#include "MemFileUtils.h"
#include "TigerRead.h"
#include "gshhs.h"
#include "TigerProcess.h"
#include "TigerImport.h"
#include "VPFImport.h"
#include "CompGeomUtils.h"
#include "ConfigSystem.h"
#include <ctype.h>

static void	import_tiger_repository(const string& rt)
{
	string root(rt);
	int	fnum = -1;
	if ((root.length() > 3) &&
		(root.substr(root.length()-4)==".RT1" ||
		 root.substr(root.length()-4)==".rt1"))
	{
		root = root.substr(0, root.length()-4);
		fnum = atoi(root.c_str() + root.length() - 5);
		root.erase(root.rfind('/'));
	}
	if ((root.length() > 3) &&
		(root.substr(root.length()-4)==".zip" ||
		 root.substr(root.length()-4)==".ZIP"))
	{
		fnum = atoi(root.c_str() + root.length() - 9);
	}
	
	if (fnum == -1)
	{
		fprintf(stderr,"Could not identify file %s as a TIGER file.\n", root.c_str());
	} else {

		MFFileSet * fs = FileSet_Open(root.c_str());
		if (fs)
		{	
			printf("Reading %s/TGR%05d.RT1\n", root.c_str(), fnum);
			TIGER_LoadRT1(fs, fnum);
			printf("Reading %s/TGR%05d.RT2\n", root.c_str(), fnum);
			TIGER_LoadRT2(fs, fnum);
			printf("Reading %s/TGR%05d.RTP\n", root.c_str(), fnum);
			TIGER_LoadRTP(fs, fnum);
			printf("Reading %s/TGR%05d.RTI\n", root.c_str(), fnum);
			TIGER_LoadRTI(fs, fnum);
			printf("Reading %s/TGR%05d.RT7\n", root.c_str(), fnum);
			TIGER_LoadRT7(fs, fnum);
			printf("Reading %s/TGR%05d.RT8\n", root.c_str(), fnum);
			TIGER_LoadRT8(fs, fnum);
							
			FileSet_Close(fs);
		} else 
			fprintf(stderr,"Could not open %s as a file set.\n", root.c_str());
	}
}


static int DoSDTSImport(const vector<const char *>& args)
{
	try {
		ImportSDTSTransferTVP(args[0], NULL, gMap);
	} catch(SDTSException& e) {
		fprintf(stderr,"SDTS Got exception: %s\n", e.what());
		return 1;
	}
	return 0;
}

static int DoTigerIndex(const vector<const char *>& args)
{
		TigerMap	tigerMap;

	tigerMap.clear();
	ReadTigerIndex(args[0], tigerMap);
	if (tigerMap.empty()) 
	{
		fprintf(stderr,"Could not open tiger index %s\n",args[0]);
		return 1;
	} else {
		int	hashNum = gMapSouth * 360 + gMapWest;
		FileList& fl = tigerMap[hashNum];
		if (fl.empty())
		{
			fprintf(stderr,"No tiger files available for %d,%d\n", gMapSouth, gMapWest);
			return 1;
		}
		string	partial(args[0]);
		string::size_type div = partial.rfind('/');
		if (div != partial.npos)
			partial.erase(div+1);
		for (int n = 0; n < fl.size(); ++n)
		{
			string	full = partial + fl[n].name;
			import_tiger_repository(full);
		}
	}
	
	if (gVerbose) printf("Sorting...\n");
	{
		TIGER_EliminateZeroLengthShapePoints();
		TIGER_RoughCull(gMapWest, gMapSouth, gMapEast, gMapNorth);
		if (gChains.size() > 0)
			TIGER_PostProcess(gMap);
		else
			printf("Skipping post process - no chains.\n");
	}
	
	if (gVerbose)  printf("Read: %d chains, %d landmarks, %d polygons.\n",
		gChains.size(), gLandmarks.size(), gPolygons.size());

	if (gChains.size() > 0)
	{
//		StElapsedTime	timer("Importing");
		TIGERImport(gChains, gLandmarks, gPolygons, gMap, gProgress);
	} else 
		printf("Skipping post process - no chains.\n");			

	if (gVerbose) printf("Map contains: %d faces, %d half edges, %d vertices.\n",
								gMap.number_of_faces(),
								gMap.number_of_halfedges(),
								gMap.number_of_vertices());
	return 0;
}
		

static int DoTigerImport(const vector<const char *>& args)
{
	for (int n = 0; n < args.size(); ++n)
	{
		string	root = args[n];
		import_tiger_repository(root);				
	}

	if (gVerbose) printf("Sorting...\n");
	{
		TIGER_EliminateZeroLengthShapePoints();
		TIGER_RoughCull(gMapWest, gMapSouth, gMapEast, gMapNorth);		
		if (gChains.size() > 0)
			TIGER_PostProcess(gMap);
		else
			printf("Skipping post process - no chains.\n");
	}
	
	if (gVerbose)  printf("Read: %d chains, %d landmarks, %d polygons.\n",
		gChains.size(), gLandmarks.size(), gPolygons.size());

	if (gChains.size() > 0)
	{
//		StElapsedTime	timer("Importing");
		TIGERImport(gChains, gLandmarks, gPolygons, gMap, gProgress);
	} else 
		printf("Skipping post process - no chains.\n");			


	if (gVerbose) printf("Map contains: %d faces, %d half edges, %d vertices.\n",
								gMap.number_of_faces(),
								gMap.number_of_halfedges(),
								gMap.number_of_vertices());
	return 0;
}

struct	VPFCoverageInfo_t {
	int						topology;
	vector<VPF_FaceRule_t>	face_rules;
	vector<VPF_LineRule_t>	line_rules;
	vector<int>				trans_flags;
};

static map<string, VPFCoverageInfo_t>	sVPFRules;

bool	ReadVPFLine(const vector<string>& tokens, void * ref)
{
		string				coverage;
		int					topology, flag, feature;
		VPF_FaceRule_t		face_rule;
		VPF_LineRule_t		line_rule;

	if (tokens[0] == "VPF_COVERAGE")
	{
		if (TokenizeLine(tokens, " si", &coverage, &topology) != 3) return false;
		sVPFRules[coverage].topology = topology;
		
	} 
	else if (tokens[0] == "VPF_FACE_RULE")
	{
		if (TokenizeLine(tokens, " sttttiee", &coverage, 
			&face_rule.table,
			&face_rule.attr_column,
			&face_rule.ref_column,
			&face_rule.strval,
			&face_rule.ival,
			&face_rule.terrain_type,
			&face_rule.area_feature) != 9) return false;
		sVPFRules[coverage].face_rules.push_back(face_rule);
	} 
	else if (tokens[0] == "VPF_LINE_RULE")
	{
		if (TokenizeLine(tokens, " sttttiei", &coverage, 
			&line_rule.table,
			&line_rule.attr_column,
			&line_rule.ref_column,
			&line_rule.strval,
			&line_rule.ival,
			&line_rule.he_param,
			&line_rule.he_trans_flags) != 9) return false;
		sVPFRules[coverage].line_rules.push_back(line_rule);
	} 
	else if (tokens[0] == "VPF_TRANS_FLAGS")
	{
		if (TokenizeLine(tokens, " sie", &coverage, &flag, &feature) != 4) return false;
		sVPFRules[coverage].trans_flags.push_back(flag);
		sVPFRules[coverage].trans_flags.push_back(feature);
	} else
		return false;
	return true;
}


static int DoVPFImport(const vector<const char *>& args)
{
	static bool first_time = true;
	if (first_time)
	{
		first_time = false;
		RegisterLineHandler("VPF_COVERAGE", ReadVPFLine, NULL);
		RegisterLineHandler("VPF_FACE_RULE", ReadVPFLine, NULL);
		RegisterLineHandler("VPF_LINE_RULE", ReadVPFLine, NULL);
		RegisterLineHandler("VPF_TRANS_FLAGS", ReadVPFLine, NULL);
		LoadConfigFile("vmap0_import.txt");
		
		for (map<string, VPFCoverageInfo_t>::iterator rules = sVPFRules.begin(); rules != sVPFRules.end(); ++rules)
		{
			rules->second.trans_flags.push_back(0);
			rules->second.face_rules.push_back(VPF_FaceRule_t());
			rules->second.line_rules.push_back(VPF_LineRule_t());
			rules->second.face_rules.back().table = NULL;
			rules->second.line_rules.back().table = NULL;
		}
	}
			
	// VMap uses this funky lettering scheme - they drop
	// the letters i and o to avoid confusion with 1 and 0.
	static char chrs[] = { "abcdefghjklmnpqrstuvwxyz" };

	const char * cov_dir = args[0];
	const char * cov_list = args[1];
	
	int west = (180 + atoi(args[2]) ) / 15;
	int south = (90 + atoi(args[3])) / 15;
	
	char	tile[5];
	char	coverage[1024];
	tile[0] = chrs[west];
	tile[1] = toupper(chrs[south]);
	tile[2] = 0;
	if (args.size() > 4)
	{
		tile[1] = (chrs[south]);
		tile[2] = chrs[atoi(args[4])];
		tile[3] = chrs[atoi(args[5])];
		tile[4] = 0;
	}

	char	coverages[512];
	char *	cov = coverages, * found;
	strcpy(coverages,cov_list);
	bool	first = true;
	bool	ok;
	while ((found = strtok(cov, ",")) != NULL)
	{
		if (sVPFRules.count(found) == 0)
		{
			printf("Error - don't know to import %s\n", found);
			return 1;
		}
		strcpy(coverage, cov_dir);
		strcat(coverage, found);
		if (first || sVPFRules[found].topology != 3)
		{
			ok = VPFImportTopo3(coverage, tile, gMap,
					sVPFRules[found].topology == 3,
					&*sVPFRules[found].line_rules.begin(),
					&*sVPFRules[found].face_rules.begin(),
					&*sVPFRules[found].trans_flags.begin());
					
			if (ok && first)
			{
				Point2	sw, ne;
				CalcBoundingBox(gMap, sw, ne);
				for (int x = sw.x + 1; x < ne.x; ++x)
				{
					gMap.insert_edge(Point2(x,sw.y),Point2(x,ne.y), NULL, NULL);				
				}
				for (int y = sw.y + 1; y < ne.y; ++y)
				{
					gMap.insert_edge(Point2(sw.x,y),Point2(ne.x,y), NULL, NULL);
				}
			}
		} else {
			Pmwx	overlay;
			ok = VPFImportTopo3(coverage, tile, overlay,
					sVPFRules[found].topology == 3,
					&*sVPFRules[found].line_rules.begin(),
					&*sVPFRules[found].face_rules.begin(),
					&*sVPFRules[found].trans_flags.begin());
			if (gVerbose) printf("Merging maps  Dst faces = %d, src hedges = %d\n", gMap.number_of_faces(), overlay.number_of_halfedges());
			MergeMaps(gMap, overlay, true, NULL, false);
		}
				
		if (ok)
		{
			if (gVerbose) printf("Imported %s/%s\n", cov, tile);
			if (gVerbose) printf("Map contains: %d faces, %d half edges, %d vertices.\n",
				gMap.number_of_faces(),
				gMap.number_of_halfedges(),
 				gMap.number_of_vertices());
//			PERFORMANCE NOTE: line insertion is a function of the averge numer of segments in a CCB.
//			Simplifying makes fewer complex large faces - this makes insert a lot slower.
//			SimplifyMap(gMap);
//			if (gVerbose) printf("(Post simplify: %d faces, %d half edges, %d vertices.)\n",
//				gMap.number_of_faces(),
//				gMap.number_of_halfedges(),
//				gMap.number_of_vertices());
		} else {
			fprintf(stderr, "ERROR importing VPF file %s/%s\n", cov, tile);
			return 1;
		}
		cov = NULL;
		first = false;
	}

	return 0;
}

static int DoGSHHSImport(const vector<const char *>& args)
{
	double c[4] = { gMapWest, gMapSouth, gMapEast, gMapNorth };
	if (ImportGSHHS(args[0], gMap, c))
	{
		if (gVerbose)printf("Map contains: %d faces, %d half edges, %d vertices.\n",
			gMap.number_of_faces(),
			gMap.number_of_halfedges(),
			gMap.number_of_vertices());
	} else {
		printf("Error importing GSHHS file %s\n", args[0]);
		return 1;
	}
	return 0;
}

static int DoTigerBounds(const vector<const char *>& args)
{
		TigerMap	tigerMap;

	ReadTigerIndex(args[0], tigerMap);
	if (tigerMap.empty()) {
		fprintf(stderr,"Could not open tiger index %s\n",args[0]);
		return 1;
	} else {
		float	lat_min = 90.0;
		float	lat_max =-90.0;
		float	lon_min = 180.0;
		float	lon_max =-180.0;
		for (TigerMap::iterator i = tigerMap.begin(); i != tigerMap.end(); ++i)
		{
			for (FileList::iterator f = i->second.begin(); f != i->second.end(); ++f)
			{
				lat_min = min(lat_min, f->lat_min);
				lat_max = max(lat_max, f->lat_max);
				lon_min = min(lon_min, f->lon_min);
				lon_max = max(lon_max, f->lon_max);
			}
		}
		int iymin = floor(lat_min);
		int iymax = ceil(lat_max);
		int ixmin = floor(lon_min);
		int ixmax = ceil(lon_max);
		printf("Total bounds: %d,%d to %d,%d\n", ixmin, iymin, ixmax, iymax);
		int count = 0;
		for (int y = iymin; y <= iymax; ++y)
		for (int x = ixmin; x <= ixmax; ++x)
		{
			int hash = y * 360 + x;
			if (tigerMap.find(hash) != tigerMap.end())
				++count, printf("call make_us_one_vec %d %d %d %d %+03d%+04d %+03d%+04d.xes\n",x,y,x+1,y+1,latlon_bucket(y), latlon_bucket(x), y,x);
		}
		printf("Total = %d\n", count);
	}
	return 0;
}

static int DoShapeImport(const vector<const char *>& args)
{
	gMap.clear();
	
	SHPHandle file = SHPOpen(args[0], "rb");
	if (file == NULL)
	{
		fprintf(stderr, "Could not open shape file %s\n", args[0]);
		return 1;
	}
	
	int	entityCount, shapeType;
	double	bounds_lo[4], bounds_hi[4];
	
	SHPGetInfo(file, &entityCount, &shapeType, bounds_lo, bounds_hi);
	
	for (int n = 0; n < entityCount; ++n)
	{
		SHPObject * obj = SHPReadObject(file, n);

		if (obj->nSHPType == SHPT_POLYGONZ || obj->nSHPType == SHPT_POLYGON ||
			obj->nSHPType == SHPT_POLYGONM)
		{
			for (int part = 0; part < obj->nParts; ++part)
			{
				Polygon2 pts;
				int start_idx = obj->panPartStart[part];
				int stop_idx = ((part+1) == obj->nParts) ? obj->nVertices : obj->panPartStart[part+1];
				for (int index = start_idx; index < stop_idx; ++index)
				{
					// SHAPE FILE QUIRK: they use CW polygons.  So part 0 (usually the outer bounds
					// of a polygon) is CW.
					if (part == 0)
						pts.insert(pts.begin(), Point2(obj->padfX[index],obj->padfY[index]));
					else
						pts.insert(pts.end(), Point2(obj->padfX[index],obj->padfY[index]));
				}
				DebugAssert(pts.front() == pts.back());
				pts.pop_back();
				
//				SimplifyPolygonMaxMove(pts, cos(pts[0].y * DEG_TO_RAD) * NM_TO_DEG_LAT * MTR_TO_NM * 30.0, true, true);
				MidpointSimplifyPolygon(pts);
				
				Pmwx::Locate_type loc;
				GISHalfedge * parent = gMap.locate_point(pts[0], loc);
				DebugAssert(loc == Pmwx::locate_Face);
				GISFace * face = (parent == NULL) ? gMap.unbounded_face() : parent->face();
				GISFace * new_face = gMap.insert_ring(face, pts);
				if (part == 0)	new_face->mTerrainType = terrain_Water;
				else			new_face->mTerrainType = terrain_Natural;
			}
		}

		SHPDestroyObject(obj);	
	}
	
	
	SHPClose(file);
	return 0;
}	

static	GISTool_RegCmd_t		sVectorCmds[] = {
{ "-sdts", 			1, 1, 	DoSDTSImport, 			"Import SDTS VTP vector map.", "" },
{ "-tiger", 		1, -1, 	DoTigerImport, 			"Import tiger line file.", "" },
{ "-tigerindex", 	1, 1, 	DoTigerIndex, 			"Import tiger line files.", "" },
{ "-tigerbounds", 	1, 1, 	DoTigerBounds, 			"Show all tiger files for a given location.", "" },
{ "-vpf", 			4, 6, 	DoVPFImport, 			"Import VPF coverage <path> <coverages> <lon> <lat> [<sublon> <sublat>]", "" },
{ "-gshhs", 		1, 1, 	DoGSHHSImport, 			"Import GSHHS shorelines.", "" },
{ "-shapefile", 	1, 1, 	DoShapeImport, 			"Import ESRI Shape File.", "" },
{ 0, 0, 0, 0, 0, 0 }
};

void	RegisterVectorCmds(void)
{
	GISTool_RegisterCommands(sVectorCmds);
}
