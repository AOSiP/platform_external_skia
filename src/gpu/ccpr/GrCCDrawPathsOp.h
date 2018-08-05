/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrCCDrawPathsOp_DEFINED
#define GrCCDrawPathsOp_DEFINED

#include "GrShape.h"
#include "SkTInternalLList.h"
#include "ccpr/GrCCSTLList.h"
#include "ops/GrDrawOp.h"

struct GrCCPerFlushResourceSpecs;
class GrCCAtlas;
class GrOnFlushResourceProvider;
class GrCCPathCache;
class GrCCPathCacheEntry;
class GrCCPerFlushResources;
class GrCCPerOpListPaths;

/**
 * This is the Op that draws paths to the actual canvas, using atlases generated by CCPR.
 */
class GrCCDrawPathsOp : public GrDrawOp {
public:
    DEFINE_OP_CLASS_ID
    SK_DECLARE_INTERNAL_LLIST_INTERFACE(GrCCDrawPathsOp);

    static std::unique_ptr<GrCCDrawPathsOp> Make(GrContext*, const SkIRect& clipIBounds,
                                                 const SkMatrix&, const GrShape&,
                                                 const SkRect& devBounds, GrPaint&&);
    ~GrCCDrawPathsOp() override;

    const char* name() const override { return "GrCCDrawPathsOp"; }
    FixedFunctionFlags fixedFunctionFlags() const override { return FixedFunctionFlags::kNone; }
    RequiresDstTexture finalize(const GrCaps&, const GrAppliedClip*) override;
    bool onCombineIfPossible(GrOp*, const GrCaps&) override;
    void visitProxies(const VisitProxyFunc& fn) const override { fProcessors.visitProxies(fn); }
    void onPrepare(GrOpFlushState*) override {}

    void wasRecorded(GrCCPerOpListPaths* owningPerOpListPaths);

    // Makes decisions about how to draw each path (cached, copied, rendered, etc.), and
    // increments/fills out the corresponding GrCCPerFlushResourceSpecs. 'stashedAtlasKey', if
    // valid, references the mainline coverage count atlas from the previous flush. Paths found in
    // this atlas will be copied to more permanent atlases in the resource cache.
    void accountForOwnPaths(GrCCPathCache*, GrOnFlushResourceProvider*,
                            const GrUniqueKey& stashedAtlasKey, GrCCPerFlushResourceSpecs*);

    // Allows the caller to decide whether to copy paths out of the stashed atlas and into the
    // resource cache, or to just re-render the paths from scratch. If there aren't many copies or
    // the copies would only fill a small atlas, it's probably best to just re-render.
    enum class DoCopiesToCache : bool {
        kNo = false,
        kYes = true
    };

    // Allocates the GPU resources indicated by accountForOwnPaths(), in preparation for drawing. If
    // DoCopiesToCache is kNo, the paths slated for copy will instead be re-rendered from scratch.
    //
    // NOTE: If using DoCopiesToCache::kNo, it is the caller's responsibility to call
    //       convertCopiesToRenders() on the GrCCPerFlushResourceSpecs.
    void setupResources(GrOnFlushResourceProvider*, GrCCPerFlushResources*, DoCopiesToCache);

    void onExecute(GrOpFlushState*) override;

private:
    friend class GrOpMemoryPool;

    enum class Visibility {
        kPartial,
        kMostlyComplete,  // (i.e., can we cache the whole path mask if we think it will be reused?)
        kComplete
    };

    GrCCDrawPathsOp(const SkMatrix&, const GrShape&, const SkIRect& shapeDevIBounds,
                    const SkIRect& maskDevIBounds, Visibility maskVisibility,
                    const SkRect& devBounds, GrPaint&&);

    void recordInstance(GrTextureProxy* atlasProxy, int instanceIdx);

    const SkMatrix fViewMatrixIfUsingLocalCoords;

    struct SingleDraw {
        SingleDraw(const SkMatrix&, const GrShape&, const SkIRect& shapeDevIBounds,
                   const SkIRect& maskDevIBounds, Visibility maskVisibility, GrColor);
        ~SingleDraw();

        SkMatrix fMatrix;
        const GrShape fShape;
        const SkIRect fShapeDevIBounds;
        SkIRect fMaskDevIBounds;
        Visibility fMaskVisibility;
        GrColor fColor;

        sk_sp<GrCCPathCacheEntry> fCacheEntry;
        sk_sp<GrTextureProxy> fCachedAtlasProxy;
        SkIVector fCachedMaskShift;

        SingleDraw* fNext = nullptr;
    };

    GrCCSTLList<SingleDraw> fDraws;
    SkDEBUGCODE(int fNumDraws = 1);

    GrCCPerOpListPaths* fOwningPerOpListPaths = nullptr;
    GrProcessorSet fProcessors;

    struct InstanceRange {
        GrTextureProxy* fAtlasProxy;
        int fEndInstanceIdx;
    };

    SkSTArray<2, InstanceRange, true> fInstanceRanges;
    int fBaseInstance SkDEBUGCODE(= -1);
};

#endif
