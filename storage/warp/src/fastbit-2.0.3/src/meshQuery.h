// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_MESHQUERY_H
#define IBIS_MESHQUERY_H
///@file
/// The header file defining an extension of query on mesh data.
///
#include "query.h"

namespace ibis { // extend ibis name space
    class meshQuery;
} // namespace

/// The class adds more functionality to ibis::query to handle data from
/// regular meshes.  The new functions treats cells of meshes as connected
/// regions in space.
class FASTBIT_CXX_DLLSPEC ibis::meshQuery : public ibis::query {
 public:
    virtual ~meshQuery();
    /// Constructor for building a new query.
    meshQuery(const char* uid, const part* et, const char* pref=0);
    /// Constructor for recoverying from crash.
    meshQuery(const char* dir, const ibis::partList& tl) : query(dir, tl) {};

    int getHitsAsLines(std::vector<uint32_t>& lines) const {
	return getHitsAsLines(lines, partition()->getMeshShape());}
    int getHitsAsLines(std::vector<uint32_t>& lines,
		       const std::vector<uint32_t>& dim) const;
    static int labelLines(uint32_t nd,
			  const std::vector<uint32_t>& lines,
			  std::vector<uint32_t>& labels);

    int  getHitsAsBlocks(std::vector< std::vector<uint32_t> >& reg,
			 const bool merge=false) const;
    int  getHitsAsBlocks(std::vector< std::vector<uint32_t> >& reg,
			 const std::vector<uint32_t>& dim,
			 const bool merge=false) const;

    int  getPointsOnBoundary(std::vector< std::vector<uint32_t> >& bdy,
			     const std::vector<uint32_t>& dim) const;
    int  getPointsOnBoundary(std::vector< std::vector<uint32_t> >& bdy) const;

    static int bitvectorToCoordinates(const ibis::bitvector& bv,
				      const std::vector<uint32_t>& dim,
				      std::vector<uint32_t>& coords);
    static int labelBlocks
	(const std::vector< std::vector<uint32_t> >& blocks,
	 std::vector<uint32_t>& labels);

 protected:
    int linesIn1D(std::vector<uint32_t>& lines) const;
    int linesIn2D(std::vector<uint32_t>& lines,
		  const std::vector<uint32_t>& dim) const;
    int linesIn3D(std::vector<uint32_t>& lines,
		  const std::vector<uint32_t>& dim) const;
    int linesIn4D(std::vector<uint32_t>& lines,
		  const std::vector<uint32_t>& dim) const;
    int linesInND(std::vector<uint32_t>& lines,
		  const std::vector<uint32_t>& dim) const;

    static int labelLines1(const std::vector<uint32_t>& lines,
			   std::vector<uint32_t>& labels);
    static int labelLines2(const std::vector<uint32_t>& lines,
			   std::vector<uint32_t>& labels);
    static int labelLines3(const std::vector<uint32_t>& lines,
			   std::vector<uint32_t>& labels);
    static int labelLines4(const std::vector<uint32_t>& lines,
			   std::vector<uint32_t>& labels);
    static int labelLinesN(uint32_t nd,
			   const std::vector<uint32_t>& lines,
			   std::vector<uint32_t>& labels);

    int  toBlocks1(const ibis::bitvector& bv,
		  std::vector< std::vector<uint32_t> >& reg) const;
    int  toBlocks2(const ibis::bitvector& bv,
		   const std::vector<uint32_t>& dim,
		   std::vector< std::vector<uint32_t> >& reg) const;
    int  toBlocks3(const ibis::bitvector& bv,
		   const std::vector<uint32_t>& dim,
		   std::vector< std::vector<uint32_t> >& reg) const;
    int  toBlocksN(const ibis::bitvector& bv,
		   const std::vector<uint32_t>& dim,
		   std::vector< std::vector<uint32_t> >& reg) const;
    void block2d(uint32_t last, const std::vector<uint32_t>& dim,
		 std::vector<uint32_t>& block,
		 std::vector< std::vector<uint32_t> >& reg) const;
    void block3d(uint32_t last, const uint32_t n2, const uint32_t n3,
		 const std::vector<uint32_t>& dim,
		 std::vector<uint32_t>& block,
		 std::vector< std::vector<uint32_t> >& reg) const;
    void blocknd(uint32_t last,
		 const std::vector<uint32_t>& scl,
		 const std::vector<uint32_t>& dim,
		 std::vector<uint32_t>& block,
		 std::vector< std::vector<uint32_t> >& reg) const;
    void merge2DBlocks(std::vector< std::vector<uint32_t> >& reg) const;
    void merge3DBlocks(std::vector< std::vector<uint32_t> >& reg) const;
    void mergeNDBlocks(std::vector< std::vector<uint32_t> >& reg) const;

    int  findPointsOnBoundary(const ibis::bitvector& bv,
			      const std::vector<uint32_t>& dim,
			      std::vector< std::vector<uint32_t> >& bdy) const;
    void boundary2d(const std::vector<uint32_t>& dim,
		    const std::vector< std::vector<uint32_t> >& rang,
		    std::vector< std::vector<uint32_t> >& bdy) const;
    void boundary2d1(const std::vector<uint32_t>& dim,
		    const std::vector< std::vector<uint32_t> >& rang,
		    std::vector< std::vector<uint32_t> >& bdy) const;
    void boundary3d(const std::vector<uint32_t>& dim,
		    const std::vector< std::vector<uint32_t> >& rang,
		    std::vector< std::vector<uint32_t> >& bdy) const;
    void boundarynd(const std::vector<uint32_t>& dim,
		    const std::vector< std::vector<uint32_t> >& rang,
		    std::vector< std::vector<uint32_t> >& bdy) const;

    static uint32_t afind(ibis::array_t<uint32_t>& rep, uint32_t s);
    static void aset(ibis::array_t<uint32_t>& rep,
		     uint32_t s, uint32_t r);
    static uint32_t aflatten(ibis::array_t<uint32_t>& rep);
    static int label1DBlocks
	(const std::vector< std::vector<uint32_t> >& blocks,
	 std::vector<uint32_t>& labels);
    static int label2DBlocks
	(const std::vector< std::vector<uint32_t> >& blocks,
	 std::vector<uint32_t>& labels);
    static int label3DBlocks
	(const std::vector< std::vector<uint32_t> >& blocks,
	 std::vector<uint32_t>& labels);
    static int label4DBlocks
	(const std::vector< std::vector<uint32_t> >& blocks,
	 std::vector<uint32_t>& labels);

 private:
    meshQuery();
    meshQuery(const meshQuery&);
    meshQuery& operator=(const meshQuery&);
}; // class ibis::meshQuery
#endif // IBIS_MESHQUERY_H
