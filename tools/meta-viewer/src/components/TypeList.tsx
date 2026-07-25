import type { TypeMeta } from "../parser";
import TypeCard from "./TypeCard";
import styles from "./TypeList.module.css";

interface Props {
  types: TypeMeta[];
}

export default function TypeList({ types }: Props) {
  if (types.length === 0) {
    return <div className={styles.empty}>No types found in metadata.</div>;
  }

  return (
    <div className={styles.list}>
      <div className={styles.title}>
        Types <span className={styles.count}>{types.length}</span>
      </div>
      {types.map((t, i) => (
        <TypeCard key={i} type={t} />
      ))}
    </div>
  );
}