interface TreeNode {
  key: string;
  label: string;
  data: any;
  children?: TreeNode[];
  selectable?: boolean;
}

const nodes: TreeNode[] = [];
function buildTreeNodes(...): TreeNode[] {}

function getNodeRange(node: TreeNode) { ... }

const keys: { [key: string]: boolean } = {};