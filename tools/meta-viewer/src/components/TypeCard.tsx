import { useState } from "react";
import type { TypeMeta } from "../parser";
import { getTokenName } from "../parser";
import MemberList from "./MemberList";
import styles from "./TypeCard.module.css";

interface Props {
  type: TypeMeta;
}

export default function TypeCard({ type }: Props) {
  const [expanded, setExpanded] = useState(false);

  return (
    <div className={styles.card}>
      <div className={styles.header} onClick={() => setExpanded(!expanded)}>
        <span className={`${styles.arrow} ${expanded ? styles.arrowOpen : ""}`}>
          &#9654;
        </span>
        <span className={styles.typeName}>{type.className}</span>
        <span className={styles.baseName}>
          {type.baseName && type.baseName !== "Base" ? ` : ${type.baseName}` : ""}
        </span>
        <span className={styles.spacer} />
        <span className={styles.badge} title={`Type ID: ${type.typeID}`}>
          ID: {type.typeID}
        </span>
        <span className={styles.badge}>
          {type.memberCount} member{type.memberCount !== 1 ? "s" : ""}
        </span>
      </div>
      {expanded && (
        <div className={styles.body}>
          <div className={styles.meta}>
            <span className={styles.metaLabel}>Type ID:</span>
            <span className={styles.metaValue}>
              {type.typeID} ({getTokenName(type.typeID)})
            </span>
          </div>
          <div className={styles.meta}>
            <span className={styles.metaLabel}>Base:</span>
            <span className={styles.metaValue}>
              {type.baseName || "(none)"}
            </span>
          </div>
          <div className={styles.membersTitle}>Members</div>
          <MemberList members={type.members} />
        </div>
      )}
    </div>
  );
}