/***************************************************************************
                         qgsfeaturesource.cpp
                         -------------------
    begin                : May 2017
    copyright            : (C) 2017 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsfeaturesource.h"
#include "qgsfeaturerequest.h"
#include "qgsfeatureiterator.h"
#include "qgsmemoryproviderutils.h"
#include "qgsfeedback.h"
#include "qgsvectorlayer.h"
#include "qgsvectordataprovider.h"

QgsFeatureSource::FeatureAvailability QgsFeatureSource::hasFeatures() const
{
  return FeaturesMaybeAvailable;
}

QSet<QVariant> QgsFeatureSource::uniqueValues( int fieldIndex, int limit ) const
{
  if ( fieldIndex < 0 || fieldIndex >= fields().count() )
    return QSet<QVariant>();

  QgsFeatureRequest req;
  req.setFlags( QgsFeatureRequest::NoGeometry );
  req.setSubsetOfAttributes( QgsAttributeList() << fieldIndex );

  QSet<QVariant> values;
  QgsFeatureIterator it = getFeatures( req );
  QgsFeature f;
  while ( it.nextFeature( f ) )
  {
    values.insert( f.attribute( fieldIndex ) );
    if ( limit > 0 && values.size() >= limit )
      return values;
  }
  return values;
}

QVariant QgsFeatureSource::minimumValue( int fieldIndex ) const
{
  if ( fieldIndex < 0 || fieldIndex >= fields().count() )
    return QVariant();

  QgsFeatureRequest req;
  req.setFlags( QgsFeatureRequest::NoGeometry );
  req.setSubsetOfAttributes( QgsAttributeList() << fieldIndex );

  QVariant min;
  QgsFeatureIterator it = getFeatures( req );
  QgsFeature f;
  while ( it.nextFeature( f ) )
  {
    QVariant v = f.attribute( fieldIndex );
    if ( v.isValid() && qgsVariantLessThan( v, min ) )
    {
      min = v;
    }
  }
  return min;
}

QVariant QgsFeatureSource::maximumValue( int fieldIndex ) const
{
  if ( fieldIndex < 0 || fieldIndex >= fields().count() )
    return QVariant();

  QgsFeatureRequest req;
  req.setFlags( QgsFeatureRequest::NoGeometry );
  req.setSubsetOfAttributes( QgsAttributeList() << fieldIndex );

  QVariant max;
  QgsFeatureIterator it = getFeatures( req );
  QgsFeature f;
  while ( it.nextFeature( f ) )
  {
    QVariant v = f.attribute( fieldIndex );
    if ( v.isValid() && qgsVariantGreaterThan( v, max ) )
    {
      max = v;
    }
  }
  return max;
}

QgsRectangle QgsFeatureSource::sourceExtent() const
{
  QgsRectangle r;

  QgsFeatureRequest req;
  req.setSubsetOfAttributes( QgsAttributeList() );

  QgsFeatureIterator it = getFeatures( req );
  QgsFeature f;
  while ( it.nextFeature( f ) )
  {
    if ( f.hasGeometry() )
      r.combineExtentWith( f.geometry().boundingBox() );
  }
  return r;
}

QgsFeatureIds QgsFeatureSource::allFeatureIds() const
{
  QgsFeatureIterator fit = getFeatures( QgsFeatureRequest()
                                        .setFlags( QgsFeatureRequest::NoGeometry )
                                        .setSubsetOfAttributes( QgsAttributeList() ) );

  QgsFeatureIds ids;

  QgsFeature fet;
  while ( fit.nextFeature( fet ) )
  {
    ids << fet.id();
  }

  return ids;
}

QgsVectorLayer *QgsFeatureSource::materialize( const QgsFeatureRequest &request, QgsFeedback *feedback )
{
  QgsWkbTypes::Type outWkbType = request.flags() & QgsFeatureRequest::NoGeometry ? QgsWkbTypes::NoGeometry : wkbType();
  QgsCoordinateReferenceSystem crs = request.destinationCrs().isValid() ? request.destinationCrs() : sourceCrs();

  QgsAttributeList requestedAttrs = request.subsetOfAttributes();

  QgsFields outFields;
  if ( request.flags() & QgsFeatureRequest::SubsetOfAttributes )
  {
    int i = 0;
    const QgsFields sourceFields = fields();
    for ( const QgsField &field : sourceFields )
    {
      if ( requestedAttrs.contains( i ) )
        outFields.append( field );
      i++;
    }
  }
  else
  {
    outFields = fields();
  }

  std::unique_ptr< QgsVectorLayer > layer( QgsMemoryProviderUtils::createMemoryLayer(
        sourceName(),
        outFields,
        outWkbType,
        crs ) );
  QgsFeature f;
  QgsFeatureIterator it = getFeatures( request );
  int fieldCount = fields().count();
  while ( it.nextFeature( f ) )
  {
    if ( feedback && feedback->isCanceled() )
      break;

    if ( request.flags() & QgsFeatureRequest::SubsetOfAttributes )
    {
      // remove unused attributes
      QgsAttributes attrs;
      for ( int i = 0; i < fieldCount; ++i )
      {
        if ( requestedAttrs.contains( i ) )
        {
          attrs.append( f.attributes().at( i ) );
        }
      }

      f.setAttributes( attrs );
    }

    layer->dataProvider()->addFeature( f, QgsFeatureSink::FastInsert );
  }

  return layer.release();
}

