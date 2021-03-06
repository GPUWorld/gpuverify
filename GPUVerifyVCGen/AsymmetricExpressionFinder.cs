//===-----------------------------------------------------------------------==//
//
//                GPUVerify - a Verifier for GPU Kernels
//
// This file is distributed under the Microsoft Public License.  See
// LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace GPUVerify
{
    using System.Collections.Generic;
    using Microsoft.Boogie;

    public class AsymmetricExpressionFinder : StandardVisitor
    {
        private static readonly HashSet<string> AsymmetricNamePrefixes = new HashSet<string>
        {
            "_WATCHED_OFFSET",
            "_READ_HAS_OCCURRED", "_READ_OFFSET", "_READ_VALUE",
            "_WRITE_HAS_OCCURRED", "_WRITE_OFFSET", "_WRITE_VALUE",
            "_ATOMIC_HAS_OCCURRED", "_ATOMIC_OFFSET",
            "_WRITE_READ_BENIGN_FLAG",
            "_READ_ASYNC_HANDLE",
            "_WRITE_ASYNC_HANDLE",
            "_ARRAY_OFFSET"
        };

        private bool found = false;

        public bool FoundAsymmetricExpr()
        {
            return found;
        }

        public override Variable VisitVariable(Variable node)
        {
            foreach (var prefix in AsymmetricNamePrefixes)
            {
                if (node.TypedIdent.Name.StartsWith(prefix))
                    found = true;
            }

            return node;
        }
    }
}
