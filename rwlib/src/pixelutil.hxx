// Utilities for working with mipmap and pixel data.

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

namespace rw
{

// Use this format to make a raster format compliant to a pixelCapabilities configuration.
// Returns whether the given raster format needs conversion into the new.
inline bool TransformDestinationRasterFormat(
    Interface *engineInterface,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat& dstRasterFormatOut, uint32& dstDepthOut, uint32& dstRowAlignmentOut, eColorOrdering& dstColorOrderOut, ePaletteType& dstPaletteTypeOut, void*& dstPaletteDataOut, uint32& dstPaletteSizeOut, eCompressionType& dstCompressionTypeOut,
    const pixelCapabilities& pixelCaps, bool hasAlpha
)
{
    // Check whether those are correct.
    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    uint32 dstRowAlignment = srcRowAlignment;
    eColorOrdering dstColorOrder = srcColorOrder;
    ePaletteType dstPaletteType = srcPaletteType;
    void *dstPaletteData = srcPaletteData;
    uint32 dstPaletteSize = srcPaletteSize;
    eCompressionType dstCompressionType = srcCompressionType;

    bool hasBeenModded = false;

    if ( dstCompressionType == RWCOMPRESS_DXT1 && pixelCaps.supportsDXT1 == false ||
         dstCompressionType == RWCOMPRESS_DXT2 && pixelCaps.supportsDXT2 == false ||
         dstCompressionType == RWCOMPRESS_DXT3 && pixelCaps.supportsDXT3 == false ||
         dstCompressionType == RWCOMPRESS_DXT4 && pixelCaps.supportsDXT4 == false ||
         dstCompressionType == RWCOMPRESS_DXT5 && pixelCaps.supportsDXT5 == false )
    {
        // Set proper decompression parameters.
        uint32 dxtType;

        bool isDXT = IsDXTCompressionType( dstCompressionType, dxtType );

        assert( isDXT == true );

        eRasterFormat targetRasterFormat = getDXTDecompressionRasterFormat( engineInterface, dxtType, hasAlpha );

        dstRasterFormat = targetRasterFormat;
        dstDepth = Bitmap::getRasterFormatDepth( targetRasterFormat );
        dstRowAlignment = 4;    // for good measure.
        dstColorOrder = COLOR_BGRA;
        dstPaletteType = PALETTE_NONE;
        dstPaletteData = NULL;
        dstPaletteSize = 0;

        // We decompress stuff.
        dstCompressionType = RWCOMPRESS_NONE;

        hasBeenModded = true;
    }

    if ( hasBeenModded == false )
    {
        if ( dstPaletteType != PALETTE_NONE && pixelCaps.supportsPalette == false )
        {
            // We want to do things without a palette.
            dstPaletteType = PALETTE_NONE;
            dstPaletteSize = 0;
            dstPaletteData = NULL;

            dstDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);

            hasBeenModded = true;
        }
    }

    // Check whether we even want an update.
    bool wantsUpdate = false;

    if ( srcRasterFormat != dstRasterFormat || dstDepth != srcDepth || dstColorOrder != srcColorOrder ||
         dstPaletteType != srcPaletteType || dstPaletteData != srcPaletteData || dstPaletteSize != srcPaletteSize ||
         dstCompressionType != srcCompressionType )
    {
        wantsUpdate = true;
    }

    // We give the result to the runtime, even if returning false.
    dstRasterFormatOut = dstRasterFormat;
    dstDepthOut = dstDepth;
    dstRowAlignmentOut = dstRowAlignment;
    dstColorOrderOut = dstColorOrder;
    dstPaletteTypeOut = dstPaletteType;
    dstPaletteDataOut = dstPaletteData;
    dstPaletteSizeOut = dstPaletteSize;
    dstCompressionTypeOut = dstCompressionType;

    return wantsUpdate;
}

// Alpha flag calculation helpers.
// Returns whether a original RW types only mipmap has transparent texels.
AINLINE bool rawMipmapCalculateHasAlpha(
    uint32 layerWidth, uint32 layerHeight, const void *texelSource, uint32 texelDataSize,
    eRasterFormat rasterFormat, uint32 depth, uint32 rowAlignment, eColorOrdering colorOrder, ePaletteType paletteType, const void *paletteData, uint32 paletteSize
)
{
    bool hasAlpha = false;

    // Decide whether we even can have alpha.
    // Otherwise there is no point in going through the pixels.
    if (rasterFormat == RASTER_1555 || rasterFormat == RASTER_4444 || rasterFormat == RASTER_8888)
    {
        uint32 srcRowSize = getRasterDataRowSize( layerWidth, depth, rowAlignment );

        // Alright, the raster can have alpha.
        // If we are palettized, we can just check the palette colors.
        if (paletteType != PALETTE_NONE)
        {
            // Determine whether we REALLY use all palette indice.
            uint32 palItemCount = paletteSize;

            bool *usageFlags = new bool[ palItemCount ];

            for ( uint32 n = 0; n < palItemCount; n++ )
            {
                usageFlags[ n ] = false;
            }

            // Loop through all pixels of the image.
            {
                const void *texelData = texelSource;

                for ( uint32 row = 0; row < layerHeight; row++ )
                {
                    const void *srcRowData = getConstTexelDataRow( texelSource, srcRowSize, row );

                    for ( uint32 col = 0; col < layerWidth; col++ )
                    {
                        uint8 palIndex;

                        bool hasIndex = getpaletteindex(srcRowData, paletteType, palItemCount, depth, col, palIndex);

                        if ( hasIndex && palIndex < palItemCount )
                        {
                            usageFlags[ palIndex ] = true;
                        }
                    }
                }
            }

            const void *palColorSource = paletteData;

            uint32 palFormatDepth = Bitmap::getRasterFormatDepth(rasterFormat);

            for (uint32 n = 0; n < palItemCount; n++)
            {
                if ( usageFlags[ n ] == true )
                {
                    uint8 r, g, b, a;

                    bool hasColor = browsetexelcolor(palColorSource, PALETTE_NONE, NULL, 0, n, rasterFormat, colorOrder, palFormatDepth, r, g, b, a);

                    if (hasColor && a != 255)
                    {
                        hasAlpha = true;
                        break;
                    }
                }
            }

            // Free memory.
            delete [] usageFlags;
        }
        else
        {
            // We have to process the entire image. Oh boy.
            // For that, we decide based on the main raster only.
            for ( uint32 row = 0; row < layerHeight; row++ )
            {
                const void *srcRowData = getConstTexelDataRow( texelSource, srcRowSize, row );

                for ( uint32 col = 0; col < layerWidth; col++ )
                {
                    uint8 r, g, b, a;

                    bool hasColor = browsetexelcolor(srcRowData, PALETTE_NONE, NULL, 0, col, rasterFormat, colorOrder, depth, r, g, b, a);

                    if (hasColor && a != 255)
                    {
                        hasAlpha = true;
                        break;
                    }
                }
            }
        }
    }

    return hasAlpha;
}

// Returns whether a DXT compressed mipmap has transparent data.
AINLINE bool dxtMipmapCalculateHasAlpha(
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, const void *srcTexels,
    uint32 dxtType
)
{
    const uint32 block_width = 4u;
    const uint32 block_height = 4u;

    // Calculate the count of compression blocks.
    uint32 alignedSurfWidth = ALIGN_SIZE( mipWidth, block_width );
    uint32 alignedSurfHeight = ALIGN_SIZE( mipHeight, block_height );

    uint32 compressionBlockCount = ( alignedSurfWidth * alignedSurfHeight ) / 16;

    // Process the blocks. 
    uint32 cur_block_x = 0;
    uint32 cur_block_y = 0;

    for ( uint32 n = 0; n < compressionBlockCount; n++ )
    {
        // Process this block.
        bool canBlockHaveAlpha = true;

        // Check whether this block can even have alpha.
        if ( dxtType == 1 )
        {
            const dxt1_block *dxtBlock = (const dxt1_block*)srcTexels + n;

            canBlockHaveAlpha = ( dxtBlock->col0.val <= dxtBlock->col1.val );
        }

        bool doesHaveAlpha = false;

        if ( canBlockHaveAlpha )
        {
            for ( uint32 local_y = 0; local_y < block_height; local_y++ )
            {
                for ( uint32 local_x = 0; local_x < block_width; local_x++ )
                {
                    if ( dxtType == 1 )
                    {
                        const dxt1_block *dxtBlock = (const dxt1_block*)srcTexels + n;

                        // If this pixel is on index 3, it is transparent.
                        uint32 index = fetchDXTIndexList( dxtBlock->indexList, local_x, local_y );

                        if ( index == 3 )
                        {
                            doesHaveAlpha = true;
                            break;
                        }
                    }
                    else if ( dxtType == 2 || dxtType == 3 )
                    {
                        const dxt2_3_block *dxtBlock = (const dxt2_3_block*)srcTexels + n;

                        // Here, we have individual alpha fields.
                        // That is why we have to index it properly.
                        uint32 coord_index = getDXTLocalBlockIndex( local_x, local_y );

                        uint32 alpha = dxtBlock->getAlphaForTexel( coord_index );

                        if ( alpha != 15u )
                        {
                            doesHaveAlpha = true;
                            break;
                        }
                    }
                    else if ( dxtType == 4 || dxtType == 5 )
                    {
                        const dxt4_5_block *dxtBlock = (const dxt4_5_block*)srcTexels + n;

                        // In this format there is really high quality alpha, sort of.
                        uint32 coord_index = getDXTLocalBlockIndex( local_x, local_y );

                        const uint64 *alphaList = (const uint64*)dxtBlock->alphaList;

                        uint32 alphaIndex = (uint32)indexlist_lookup( *alphaList, (uint64)coord_index, (uint64)3 );

                        uint8 theAlpha = dxt4_5_block::getAlphaByIndex( dxtBlock->alphaPreMult[0], dxtBlock->alphaPreMult[1], alphaIndex );

                        if ( theAlpha != 255u )
                        {
                            doesHaveAlpha = true;
                            break;
                        }
                    }
                }

                if ( doesHaveAlpha )
                {
                    break;
                }
            }
        }

        if ( doesHaveAlpha )
        {
            return true;
        }

        // Advance the texel pointer.
        cur_block_x += block_width;

        if ( cur_block_x >= alignedSurfWidth )
        {
            cur_block_x = 0;
            cur_block_y += block_height;
        }

        if ( cur_block_y >= alignedSurfHeight )
        {
            // We reached the end of the texture.
            break;
        }
    }

    // We found no transparent texel.
    return false;
}

inline bool calculateHasAlpha( const pixelDataTraversal& pixelData )
{
    // For everything that this library supports we should have correct alpha output.
    // Anything new has to be added ASAP.
    eCompressionType compressionType = pixelData.compressionType;

    bool hasAlpha = false;

    // Check whether we need to parse it as DXT.
    uint32 dxtType = 0;

    if ( IsDXTCompressionType( compressionType, dxtType ) )
    {
        const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

        // Check the alpha according to DXT block information.
        hasAlpha =
            dxtMipmapCalculateHasAlpha(
                mipLayer.width, mipLayer.height, mipLayer.mipWidth, mipLayer.mipHeight, mipLayer.texels,
                dxtType
            );
    }
    else if ( compressionType == RWCOMPRESS_NONE )
    {
        const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

        // We assume that the first mipmap shares the same qualities like any other mipmap.
        // It is the base layer after all.
        hasAlpha = rawMipmapCalculateHasAlpha(
            mipLayer.mipWidth, mipLayer.mipHeight, mipLayer.texels, mipLayer.dataSize,
            pixelData.rasterFormat, pixelData.depth, pixelData.rowAlignment, pixelData.colorOrder,
            pixelData.paletteType, pixelData.paletteData, pixelData.paletteSize
        );
    }

    return hasAlpha;
}

}