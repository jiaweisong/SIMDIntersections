
#ifndef INTERSECTIONFACTORY_H_
#define INTERSECTIONFACTORY_H_

#include "common.h"
#include "intersection.h"
#include "partitionedintersection.h"
#include "hscalableintersection.h"
#include "gallopingintersection.h"
#include "binarysearchintersection.h"
#include "hybridintersection.h"
#include "mediumintersection.h"
#include "widevectorintersection.h"

#ifdef __INTEL_COMPILER
/**
 * Intel does not really support C++11 so we have to do something ugly like this:
 */

std::map<std::string,cardinalityintersectionfunction> initializefactory() {
    std::map<std::string,cardinalityintersectionfunction> schemes;
    schemes[ "danhybrid" ] =  danielshybridintersectioncardinality;
    schemes[ "widevector" ] =  widevector_cardinality_intersect;
    schemes[ "danscalar" ] =  intersectioncardinality;
    schemes[ "galloping" ] =  frogintersectioncardinality;
    schemes[ "1sgalloping" ] =  onesidedgallopingintersectioncardinality;
    schemes[ "textbook" ] =  classicalintersectioncardinality;
    schemes[ "textbook2" ] =  highlyscalablewordpresscom::cardinality_intersect_scalar;
    schemes[ "hssimd" ] =  highlyscalablewordpresscom::cardinality_intersect_SIMD;
    schemes[ "danhssimd" ] =  highlyscalablewordpresscom::opti2_cardinality_intersect_SIMD;
    schemes[ "natemedium" ] =  nate_count_medium;
    schemes[ "natedanmedium" ] =  natedan_count_medium;
    schemes[ "natedanaltmedium" ] =  natedanalt_count_medium;
    schemes[ "danfarmedium" ] =  danfar_count_medium;
    schemes[ "danfarfinemedium" ] =  danfarfine_count_medium;
    schemes[ "natesimple16"] = nate_count_simple16;
    return schemes;
}


std::map<std::string,cardinalityintersectionfunction> schemes = initializefactory();

std::map<std::string,cardinalityintersectionfunctionpart> initializefactorypart() {
    std::map<std::string,cardinalityintersectionfunctionpart> partschemes;
    partschemes[ "schlegel" ] = partitioned::cardinality_intersect_partitioned;
    partschemes[ "danschlegel" ] = partitioned::faster_cardinality_intersect_partitioned;
    return partschemes;
}


std::map<std::string,cardinalityintersectionfunctionpart> partschemes = initializefactorypart();

#else

/**
 * This is the proper way to do it:
 */
std::map<std::string,cardinalityintersectionfunction> schemes = {
	{	"danhybrid",danielshybridintersectioncardinality},
	{	"widevector", widevector_cardinality_intersect},
	{	"danscalar", intersectioncardinality},
	{	"galloping",frogintersectioncardinality},
	{	"1sgalloping", onesidedgallopingintersectioncardinality},
	{	"textbook", classicalintersectioncardinality},
	{	"textbook2", highlyscalablewordpresscom::cardinality_intersect_scalar},
	{	"hssimd", highlyscalablewordpresscom::cardinality_intersect_SIMD},
	{	"danhssimd", highlyscalablewordpresscom::opti2_cardinality_intersect_SIMD},
	{	"natemedium", nate_count_medium},
	{	"natesimple16", nate_count_simple16},
	{	"natedanmedium", natedan_count_medium},
	{   "natedanaltmedium", natedanalt_count_medium},
        {   "danfarmedium", danfar_count_medium},
        {   "danfarfinemedium", danfarfine_count_medium}
};



std::map<std::string,cardinalityintersectionfunctionpart> partschemes = {
	{	"schlegel",partitioned::cardinality_intersect_partitioned},
	{	"danschlegel", partitioned::faster_cardinality_intersect_partitioned}
};

#endif

/**
 * Convenience function
 */
std::vector<std::string> allNames() {
    std::vector < std::string > ans;
    for (auto i = schemes.begin(); i != schemes.end(); ++i) {
        ans.push_back(i->first);
    }
    for (auto i = partschemes.begin(); i != partschemes.end(); ++i) {
        ans.push_back(i->first);
    }
    return ans;
}


#endif /* INTERSECTIONFACTORY_H_ */
