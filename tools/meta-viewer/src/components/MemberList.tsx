import type { MemberMeta } from "../parser";
import TokenDesc from "./TokenDesc";
import styles from "./MemberList.module.css";

interface Props {
  members: MemberMeta[];
}

export default function MemberList({ members }: Props) {
  if (members.length === 0) {
    return <div className={styles.empty}>No members</div>;
  }

  return (
    <table className={styles.table}>
      <thead>
        <tr>
          <th className={styles.colIdx}>#</th>
          <th className={styles.colName}>Member</th>
          <th className={styles.colType}>Type Descriptor</th>
        </tr>
      </thead>
      <tbody>
        {members.map((m, i) => (
          <tr key={i}>
            <td className={styles.colIdx}>{i}</td>
            <td className={styles.colName}>{m.name}</td>
            <td className={styles.colType}>
              <TokenDesc desc={m.parsedDesc} />
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}