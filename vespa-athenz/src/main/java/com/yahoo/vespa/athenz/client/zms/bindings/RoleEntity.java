// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

package com.yahoo.vespa.athenz.client.zms.bindings;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.List;

/**
 * @author mortent
 */
@JsonIgnoreProperties(ignoreUnknown = true)
public class RoleEntity {
    private final String roleName;
    private final List<Member> roleMembers;
    private final Boolean selfServe;
    private final Boolean reviewEnabled;

    @JsonCreator
    public RoleEntity(@JsonProperty("roleName") String roleName,
                      @JsonProperty("roleMembers") List<Member> roleMembers,
                      @JsonProperty("selfServe") Boolean selfServe,
                      @JsonProperty("reviewEnabled") Boolean reviewEnabled) {
        this.roleName = roleName;
        this.roleMembers = roleMembers;
        this.selfServe = selfServe;
        this.reviewEnabled = reviewEnabled;
    }

    public String roleName() {
        return roleName;
    }

    public List<Member> roleMembers() {
        return roleMembers;
    }

    public Boolean selfServe() {
        return selfServe;
    }

    public Boolean reviewEnabled() {
        return reviewEnabled;
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    public static final class Member {
        private final String memberName;
        private final boolean active;
        private final boolean approved;
        private final String auditRef;

        @JsonCreator
        public Member(@JsonProperty("memberName") String memberName, @JsonProperty("active") boolean active, @JsonProperty("approved") boolean approved, @JsonProperty("auditRef") String auditRef) {
            this.memberName = memberName;
            this.active = active;
            this.approved = approved;
            this.auditRef = auditRef;
        }

        public String memberName() {
            return memberName;
        }

        public boolean pendingApproval() {
            return !approved;
        }

        public String auditRef() {
            return auditRef;
        }
    }
}
