import { useCallback, useRef, useState } from "react";
import styles from "./FilePicker.module.css";

interface Props {
  onFileSelect: (file: File) => void;
}

export default function FilePicker({ onFileSelect }: Props) {
  const [dragOver, setDragOver] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  const handleDragOver = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    setDragOver(true);
  }, []);

  const handleDragLeave = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    setDragOver(false);
  }, []);

  const handleDrop = useCallback(
    (e: React.DragEvent) => {
      e.preventDefault();
      setDragOver(false);
      const file = e.dataTransfer.files?.[0];
      if (file) onFileSelect(file);
    },
    [onFileSelect]
  );

  const handleClick = () => {
    inputRef.current?.click();
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) onFileSelect(file);
  };

  return (
    <div
      className={`${styles.picker} ${dragOver ? styles.dragOver : ""}`}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
      onClick={handleClick}
    >
      <input
        ref={inputRef}
        type="file"
        accept=".bin"
        className={styles.input}
        onChange={handleFileChange}
      />
      <div className={styles.icon}>&#x1F4C1;</div>
      <div className={styles.text}>
        {dragOver
          ? "Drop file here"
          : "Drag & drop a .bin file here, or click to select"}
      </div>
      <div className={styles.hint}>
        StreamPunk metadata binary files (*.bin)
      </div>
    </div>
  );
}