import type { MetaHeader } from "../parser";
import styles from "./HeaderPanel.module.css";

interface Props {
  header: MetaHeader;
  fileName: string;
}

export default function HeaderPanel({ header, fileName }: Props) {
  return (
    <div className={styles.panel}>
      <div className={styles.fileName} title={fileName}>
        {fileName}
      </div>
      <div className={styles.cards}>
        <div className={styles.card}>
          <div className={styles.label}>Magic</div>
          <div className={styles.value}>
            0x{header.magic.toString(16).toUpperCase()}
          </div>
        </div>
        <div className={styles.card}>
          <div className={styles.label}>Version</div>
          <div className={styles.value}>{header.version}</div>
        </div>
        <div className={styles.card}>
          <div className={styles.label}>Type Count</div>
          <div className={styles.value}>{header.typeCount}</div>
        </div>
      </div>
    </div>
  );
}