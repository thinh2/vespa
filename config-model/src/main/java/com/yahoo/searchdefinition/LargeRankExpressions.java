// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.searchdefinition;

import com.yahoo.config.application.api.FileRegistry;

import java.util.Collection;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

public class LargeRankExpressions {
    private final Map<String, RankExpressionBody> expressions = new ConcurrentHashMap<>();
    private final FileRegistry fileRegistry;
    private final int limit;

    public LargeRankExpressions(FileRegistry fileRegistry) {
        this(fileRegistry, 8192);
    }
    public LargeRankExpressions(FileRegistry fileRegistry, int limit) {
        this.fileRegistry = fileRegistry;
        this.limit = limit;
    }

    public void add(RankExpressionBody expression) {
        String name = expression.getName();
        RankExpressionBody prev = expressions.putIfAbsent(name, expression);
        if (prev == null) {
            expression.validate();
            expression.register(fileRegistry);
        } else {
            if ( ! prev.getBlob().equals(expression.getBlob())) {
                throw new IllegalArgumentException("Rank expression '" + name +
                        "' defined twice. Previous blob with " + prev.getBlob().remaining() +
                        " bytes, while current has " + expression.getBlob().remaining() + " bytes");
            }
        }
    }
    public int limit() { return limit; }

    /** Returns a read-only list of ranking constants ordered by name */
    public Collection<RankExpressionBody> expressions() {
        return expressions.values().stream().sorted().collect(Collectors.toUnmodifiableList());
    }

}
