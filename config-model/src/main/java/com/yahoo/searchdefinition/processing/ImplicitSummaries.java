// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.searchdefinition.processing;

import java.util.logging.Level;

import com.yahoo.config.application.api.DeployLogger;
import com.yahoo.searchdefinition.RankProfileRegistry;
import com.yahoo.searchdefinition.Schema;
import com.yahoo.searchdefinition.document.Attribute;
import com.yahoo.document.PositionDataType;
import com.yahoo.searchdefinition.document.SDField;
import com.yahoo.vespa.documentmodel.DocumentSummary;
import com.yahoo.vespa.documentmodel.SummaryField;
import com.yahoo.vespa.documentmodel.SummaryTransform;
import com.yahoo.vespa.model.container.search.QueryProfiles;

import static com.yahoo.searchdefinition.document.ComplexAttributeFieldUtils.isComplexFieldWithOnlyStructFieldAttributes;

/**
 * Makes implicitly defined summaries into explicit summaries
 *
 * @author bratseth
 */
public class ImplicitSummaries extends Processor {

    public ImplicitSummaries(Schema schema, DeployLogger deployLogger, RankProfileRegistry rankProfileRegistry, QueryProfiles queryProfiles) {
        super(schema, deployLogger, rankProfileRegistry, queryProfiles);
    }

    @Override
    public void process(boolean validate, boolean documentsOnly) {
        DocumentSummary defaultSummary = schema.getSummariesInThis().get("default");
        if (defaultSummary == null) {
            defaultSummary = new DocumentSummary("default", schema);
            defaultSummary.setFromDisk(true); // TODO: Not necessarily
            schema.addSummary(defaultSummary);
        }

        for (SDField field : schema.allConcreteFields()) {
            collectSummaries(field, schema, validate);
        }
        for (DocumentSummary documentSummary : schema.getSummaries().values()) {
            documentSummary.purgeImplicits();
        }
    }

    private void addSummaryFieldSources(SummaryField summaryField, SDField sdField) {
        sdField.addSummaryFieldSources(summaryField);
    }

    private void collectSummaries(SDField field , Schema schema, boolean validate) {
        SummaryField addedSummaryField = null;

        // Implicit
        String fieldName = field.getName();
        SummaryField fieldSummaryField = field.getSummaryField(fieldName);
        if (fieldSummaryField == null && field.doesSummarying()) {
            fieldSummaryField = new SummaryField(fieldName, field.getDataType());
            fieldSummaryField.setImplicit(true);
            addSummaryFieldSources(fieldSummaryField, field);
            fieldSummaryField.addDestination("default");
            field.addSummaryField(fieldSummaryField);
            addedSummaryField = fieldSummaryField;
        }
        if (fieldSummaryField != null) {
            for (String dest : fieldSummaryField.getDestinations()) {
                DocumentSummary summary = schema.getSummary(dest);
                if (summary != null) {
                    summary.add(fieldSummaryField);
                }
            }
        }

        // Attribute prefetch
        for (Attribute attribute : field.getAttributes().values()) {
            if (attribute.getName().equals(fieldName)) {
                if (addedSummaryField != null) {
                    addedSummaryField.setTransform(SummaryTransform.ATTRIBUTE);
                }
                if (attribute.isPrefetch()) {
                    addPrefetchAttribute(attribute, field, schema);
                }
            }
        }

        if (addedSummaryField != null && isComplexFieldWithOnlyStructFieldAttributes(field)) {
            addedSummaryField.setTransform(SummaryTransform.ATTRIBUTECOMBINER);
        }

        // Position attributes
        if (field.doesSummarying()) {
            for (Attribute attribute : field.getAttributes().values()) {
                if ( ! attribute.isPosition()) continue;
                DocumentSummary attributePrefetchSummary = getOrCreateAttributePrefetchSummary(schema);
                attributePrefetchSummary.add(field.getSummaryField(PositionDataType.getDistanceSummaryFieldName(fieldName)));
                attributePrefetchSummary.add(field.getSummaryField(PositionDataType.getPositionSummaryFieldName(fieldName)));
            }
        }

        // Explicits
        for (SummaryField summaryField : field.getSummaryFields().values()) {
            // Make sure we fetch from attribute here too
            Attribute attribute = field.getAttributes().get(fieldName);
            if (attribute != null && summaryField.getTransform() == SummaryTransform.NONE) {
                summaryField.setTransform(SummaryTransform.ATTRIBUTE);
            }

            if (isValid(summaryField, schema, validate)) {
                addToDestinations(summaryField, schema);
            }
        }

    }

    private DocumentSummary getOrCreateAttributePrefetchSummary(Schema schema) {
        DocumentSummary summary = schema.getSummariesInThis().get("attributeprefetch");
        if (summary == null) {
            summary = new DocumentSummary("attributeprefetch", schema);
            schema.addSummary(summary);
        }
        return summary;
    }


    private void addPrefetchAttribute(Attribute attribute, SDField field, Schema schema) {
        if (attribute.getPrefetchValue() == null) { // Prefetch by default - unless any summary makes this dynamic
            // Check if there is an implicit dynamic definition
            SummaryField fieldSummaryField = field.getSummaryField(attribute.getName());
            if (fieldSummaryField != null && fieldSummaryField.getTransform().isDynamic()) return;

            // Check if an explicit class makes it dynamic (first is enough, as all must be the same, checked later)
            SummaryField explicitSummaryField = schema.getExplicitSummaryField(attribute.getName());
            if (explicitSummaryField != null && explicitSummaryField.getTransform().isDynamic()) return;
        }

        DocumentSummary summary = getOrCreateAttributePrefetchSummary(schema);
        SummaryField attributeSummaryField = new SummaryField(attribute.getName(), attribute.getDataType());
        attributeSummaryField.addSource(attribute.getName());
        attributeSummaryField.addDestination("attributeprefetch");
        attributeSummaryField.setTransform(SummaryTransform.ATTRIBUTE);
        summary.add(attributeSummaryField);
    }

    // Returns whether this is valid. Warns if invalid and ignorable. Throws if not ignorable.
    private boolean isValid(SummaryField summaryField, Schema schema, boolean validate) {
        if (summaryField.getTransform() == SummaryTransform.DISTANCE ||
            summaryField.getTransform() == SummaryTransform.POSITIONS) {
            int sourceCount = summaryField.getSourceCount();
            if (validate && sourceCount != 1) {
                throw newProcessException(schema.getName(), summaryField.getName(),
                                          "Expected 1 source field, got " + sourceCount + ".");
            }
            String sourceName = summaryField.getSingleSource();
            if (validate && schema.getAttribute(sourceName) == null) {
                throw newProcessException(schema.getName(), summaryField.getName(),
                                          "Summary source attribute '" + sourceName + "' not found.");
            }
            return true;
        }

        String fieldName = summaryField.getSourceField();
        SDField sourceField = schema.getConcreteField(fieldName);
        if (validate && sourceField == null) {
            throw newProcessException(schema, summaryField, "Source field '" + fieldName + "' does not exist.");
        }
        if (! sourceField.doesSummarying() &&
            ! summaryField.getTransform().equals(SummaryTransform.ATTRIBUTE) &&
            ! summaryField.getTransform().equals(SummaryTransform.GEOPOS))
        {
            // Summary transform attribute may indicate that the ilscript was rewritten to remove summary
            // by another search that uses this same field in inheritance.
            deployLogger.logApplicationPackage(Level.WARNING, "Ignoring " + summaryField + ": " + sourceField +
                                           " is not creating a summary value in its indexing statement");
            return false;
        }

        if (summaryField.getTransform().isDynamic()
            && summaryField.getName().equals(sourceField.getName())
            && sourceField.doesAttributing()) {
            Attribute attribute = sourceField.getAttributes().get(sourceField.getName());
            if (attribute != null) {
                String destinations = "document summary 'default'";
                if (summaryField.getDestinations().size()  >0) {
                    destinations = "document summaries " + summaryField.getDestinations();
                }
                deployLogger.logApplicationPackage(Level.WARNING,
                                 "Will fetch the disk summary value of " + sourceField + " in " + destinations +
                                 " since this summary field uses a dynamic summary value (snippet/bolding): Dynamic summaries and bolding " +
                                 "is not supported with summary values fetched from in-memory attributes yet. If you want to see partial updates " +
                                 "to this attribute, remove any bolding and dynamic snippeting from this field");
                // Note: The dynamic setting has already overridden the attribute map setting,
                // so we do not need to actually do attribute.setSummary(false) here
                // Also, we can not do this, since it makes it impossible to fetch this attribute
                // in another summary
            }
        }

        return true;
    }

    private void addToDestinations(SummaryField summaryField, Schema schema) {
        if (summaryField.getDestinations().size() == 0) {
            addToDestination("default", summaryField, schema);
        }
        else {
            for (String destinationName : summaryField.getDestinations())
                addToDestination(destinationName, summaryField, schema);
        }
    }

    private void addToDestination(String destinationName, SummaryField summaryField, Schema schema) {
        DocumentSummary destination = schema.getSummary(destinationName);
        if (destination == null) {
            destination = new DocumentSummary(destinationName, schema);
            schema.addSummary(destination);
            destination.add(summaryField);
        }
        else {
            SummaryField existingField= destination.getSummaryField(summaryField.getName());
            SummaryField merged = summaryField.mergeWith(existingField);
            destination.add(merged);
        }
    }

}
