using System.ComponentModel.DataAnnotations;

namespace UrbanFlowApp.Models
{
    public enum LaneType
    {
        LANE_IN = 0,
        LANE_OUT = 1
    }

    public class LaneModel
    {
        public int LocalId { get; set; }

        [Required]
        public LaneType Type { get; set; }
        public short Bearing { get; set; } = 0;
        public short SensorPin { get; set; } = -1;
        public short GreenPin { get; set; } = -1;
        public short YellowPin { get; set; } = -1;
        public short RedPin { get; set; } = -1;
    }

    public class ConnectionModel
    {
        public int Id { get; set; } 

        [Display(Name = "From Lane (Index)")]
        public int SourceLaneIdx { get; set; }

        [Display(Name = "To Lane (Index)")]
        public int TargetLaneIdx { get; set; }

        public int Weight { get; set; } = 1;
    }

    public class PhaseModel
    {
        public int DurationMs { get; set; } = 5000;
        public List<int> ActiveConnectionIndices { get; set; } = new List<int>();
    }

    public class IntersectionViewModel
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public int DefaultPhaseDurationMs { get; set; } = 3000;

        public List<LaneModel> Lanes { get; set; } = new List<LaneModel>();
        public List<ConnectionModel> Connections { get; set; } = new List<ConnectionModel>();
        public List<PhaseModel> Phases { get; set; } = new List<PhaseModel>();
        public String StatusMessage { get; set; } = "OK";
    }
}
